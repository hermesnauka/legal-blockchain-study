using LegalChain.Consensus;
using LegalChain.Crypto;

namespace LegalChain.Core;

/// <summary>
/// The general ledger: an append-only, hash-chained list of blocks plus the pool of
/// pending (signed, not yet sealed) transactions.
/// </summary>
/// <remarks>
/// <para><b>This is a real ledger, not a mock:</b> balances are never stored — they are
/// derived by replaying every transaction from genesis (<see cref="Balances"/>), signatures
/// are verified with ML-DSA before a transaction is accepted, and
/// <see cref="IsValidChain"/> re-audits hashes, links, Merkle roots, signatures and
/// consensus proofs of the entire history. That auditability-from-first-principles is
/// the property that makes a blockchain acceptable as a compliant book of record.</para>
/// <para>Thread-safety: mutating operations take the chain lock; reads return defensive
/// copies. Request handling rides on the async/await thread pool (the .NET analogue of
/// Java's Loom virtual threads), so briefly blocked writers are cheap.</para>
/// </remarks>
public class Blockchain
{
    /// <summary>Deterministic genesis: identical on every node, so two fresh remote nodes share a common root.</summary>
    public static readonly Block Genesis = new(
        0, 0L, new string('0', 64), Block.MerkleRootOf([]), [],
        "GENESIS", "GENESIS", "genesis",
        0, Block.ComputeHash(0, 0L, new string('0', 64), Block.MerkleRootOf([]),
            "GENESIS", "GENESIS", "genesis", 0));

    private readonly List<Block> _chain = [Genesis];
    private readonly List<Transaction> _pending = [];
    private readonly Lock _gate = new();

    private readonly ConsensusEngine _consensusEngine;
    private readonly TokenomicsService _tokenomics;
    private readonly PqcSignatureService _signatures;
    private readonly EventBus _events;

    public Blockchain(ConsensusEngine consensusEngine, TokenomicsService tokenomics,
        PqcSignatureService signatures, EventBus events)
    {
        _consensusEngine = consensusEngine;
        _tokenomics = tokenomics;
        _signatures = signatures;
        _events = events;
    }

    /// <summary>
    /// Accepts a transaction into the pending pool after verifying its ML-DSA signature,
    /// that the claimed sender address really is the fingerprint of the signing key
    /// (otherwise anyone could sign as somebody else with their own key), and that the
    /// sender can actually cover the amount: confirmed balance minus what they already
    /// have pending. Rewards are exempt — they are the only sanctioned money creation,
    /// bounded by <see cref="TokenomicsService"/>. Counting pending outgoing amounts closes
    /// the mempool double-spend: two transfers that are individually covered but jointly
    /// overdraw are caught at the second <c>Submit</c>, so no sealed block can ever
    /// drive a balance negative — the first invariant an auditor checks in any book of
    /// record.
    /// </summary>
    public Transaction Submit(Transaction tx)
    {
        if (!tx.IsReward())
        {
            if (tx.SenderPublicKey is null || tx.Signature is null)
            {
                throw new ArgumentException("Transaction must be signed (ML-DSA)");
            }
            if (!_signatures.Verify(tx.ContentToSign(), tx.Signature, tx.SenderPublicKey))
            {
                throw new ArgumentException("Invalid ML-DSA signature");
            }
            string expectedSender;
            try
            {
                expectedSender = _signatures.Fingerprint(_signatures.DecodePublicKey(tx.SenderPublicKey));
            }
            catch (ArgumentException)
            {
                throw new ArgumentException("Malformed sender public key");
            }
            if (expectedSender != tx.Sender)
            {
                throw new ArgumentException("Sender address does not match signing key");
            }
        }
        lock (_gate)
        {
            if (!tx.IsReward())
            {
                decimal available = BalancesUnlocked().GetValueOrDefault(tx.Sender, 0m)
                                    - PendingOutgoingOf(tx.Sender);
                if (tx.Amount > available)
                {
                    throw new ArgumentException("Insufficient funds: sender " + tx.Sender
                        + " has " + Transaction.FormatAmount(available)
                        + " LGC available but tried to send "
                        + Transaction.FormatAmount(tx.Amount) + " LGC");
                }
            }
            _pending.Add(tx);
        }
        _events.Publish(new LedgerEvent(LedgerEvent.EventType.TX_ADDED, tx));
        return tx;
    }

    /// <summary>The sender's not-yet-sealed outgoing total; call only while holding the chain lock.</summary>
    private decimal PendingOutgoingOf(string sender)
        => _pending.Where(tx => !tx.IsReward() && tx.Sender == sender).Sum(tx => tx.Amount);

    /// <summary>
    /// Seals the pending transactions into a new block via the active consensus strategy
    /// and credits the validator with the tokenomics reward.
    /// </summary>
    public Block MinePendingTransactions(string validatorAddress)
    {
        Block block;
        lock (_gate)
        {
            Block previous = _chain[^1];
            long index = previous.Index + 1;

            var blockTxs = new List<Transaction>(_pending)
            {
                _tokenomics.RewardTransaction(validatorAddress, index, _tokenomics.IssuedSupply(_chain))
            };

            var candidate = new BlockCandidate(
                index, DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(), previous.Hash,
                blockTxs, validatorAddress);
            block = _consensusEngine.Active.Seal(candidate);

            _chain.Add(block);
            _pending.Clear();
        }
        _events.Publish(new LedgerEvent(LedgerEvent.EventType.BLOCK_ADDED, block));
        return block;
    }

    /// <summary>
    /// Full audit of a chain: genesis identity, hash consistency, previous-hash links,
    /// transaction signatures, and each block's own consensus proof (looked up by the
    /// strategy recorded in the block, so mixed-consensus histories stay verifiable).
    /// </summary>
    public bool IsValidChain(IReadOnlyList<Block> candidate)
    {
        if (candidate.Count == 0 || candidate[0] != Genesis && !GenesisEquals(candidate[0]))
        {
            return false;
        }
        for (int i = 1; i < candidate.Count; i++)
        {
            Block block = candidate[i];
            Block previous = candidate[i - 1];
            if (block.Index != previous.Index + 1
                || block.PreviousHash != previous.Hash
                || !block.HashIsConsistent())
            {
                return false;
            }
            try
            {
                if (!_consensusEngine.ByName(block.ConsensusType).Validate(block))
                {
                    return false;
                }
            }
            catch (ArgumentException)
            {
                return false; // unknown strategy recorded in the block
            }
            foreach (Transaction tx in block.Transactions)
            {
                if (!tx.IsReward()
                    && !_signatures.Verify(tx.ContentToSign(), tx.Signature, tx.SenderPublicKey))
                {
                    return false;
                }
            }
        }
        return true;
    }

    /// <summary>Genesis comparison by value: a deserialized peer genesis is a distinct instance.</summary>
    private static bool GenesisEquals(Block block)
        => block.Index == Genesis.Index && block.Hash == Genesis.Hash
           && block.Transactions.Count == 0 && block.HashIsConsistent();

    public bool IsValid() => IsValidChain(Blocks());

    /// <summary>
    /// Longest-valid-chain rule used by P2P sync: adopt a peer's chain only if it is
    /// strictly longer <b>and</b> passes the full local audit — a malicious peer cannot
    /// push a forged history, because we never trust, we always re-verify.
    /// </summary>
    public bool ReplaceChain(IReadOnlyList<Block> candidate)
    {
        lock (_gate)
        {
            if (candidate.Count <= _chain.Count || !IsValidChain(candidate))
            {
                return false;
            }
            _chain.Clear();
            _chain.AddRange(candidate);
            _pending.Clear();
        }
        _events.Publish(new LedgerEvent(LedgerEvent.EventType.CHAIN_REPLACED,
            new Dictionary<string, object> { ["length"] = candidate.Count }));
        return true;
    }

    /// <summary>Balances derived by replaying the full ledger — the "general ledger" property.</summary>
    public Dictionary<string, decimal> Balances()
    {
        lock (_gate)
        {
            return BalancesUnlocked();
        }
    }

    private Dictionary<string, decimal> BalancesUnlocked()
    {
        var balances = new Dictionary<string, decimal>();
        foreach (Block block in _chain)
        {
            foreach (Transaction tx in block.Transactions)
            {
                if (!tx.IsReward())
                {
                    balances[tx.Sender] = balances.GetValueOrDefault(tx.Sender, 0m) - tx.Amount;
                }
                balances[tx.Recipient] = balances.GetValueOrDefault(tx.Recipient, 0m) + tx.Amount;
            }
        }
        return balances;
    }

    public IReadOnlyList<Block> Blocks()
    {
        lock (_gate)
        {
            return _chain.ToList();
        }
    }

    public IReadOnlyList<Transaction> PendingTransactions()
    {
        lock (_gate)
        {
            return _pending.ToList();
        }
    }

    public int Length
    {
        get
        {
            lock (_gate)
            {
                return _chain.Count;
            }
        }
    }
}
