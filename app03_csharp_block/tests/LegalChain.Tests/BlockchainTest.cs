using LegalChain.Core;

namespace LegalChain.Tests;

/// <summary>
/// The ledger invariants an auditor checks first (BE-01..BE-07): only ML-DSA-signed,
/// funded transactions enter the mempool; mining seals them with a tokenomics reward;
/// balances replay from genesis; tampering is detected by the full audit; and a peer's
/// chain is adopted only when strictly longer and fully valid.
/// </summary>
public class BlockchainTest
{
    [Fact]
    public void RejectsUnsignedAndForgedTransactions()
    {
        var node = new TestNode();
        var unsigned = new Transaction(
            Guid.NewGuid().ToString(), DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            "lcnobody", "alice", 1m, TransactionType.TRANSFER,
            new Dictionary<string, string>(), null, null);
        Assert.Throws<ArgumentException>(() => node.Blockchain.Submit(unsigned));

        // A valid signature under a key whose fingerprint is NOT the claimed sender.
        var wallet = node.NewWallet();
        Transaction impersonation = wallet.SignedTransfer("alice", 1m) with { Sender = "lcsomebodyelse" };
        Assert.Throws<ArgumentException>(() => node.Blockchain.Submit(impersonation));
        Assert.Empty(node.Blockchain.PendingTransactions());
    }

    [Fact]
    public void RejectsOverdraftIncludingPendingOutgoing()
    {
        var node = new TestNode();
        var wallet = node.NewWallet();
        node.Blockchain.MinePendingTransactions(wallet.Address); // +50 LGC confirmed

        Assert.Throws<ArgumentException>(
            () => node.Blockchain.Submit(wallet.SignedTransfer("alice", 51m)));

        // Mempool double-spend: 30 + 30 jointly overdraw a 50 LGC balance.
        node.Blockchain.Submit(wallet.SignedTransfer("alice", 30m));
        Assert.Throws<ArgumentException>(
            () => node.Blockchain.Submit(wallet.SignedTransfer("bob", 30m)));
        Assert.Single(node.Blockchain.PendingTransactions());
    }

    [Fact]
    public void MiningSealsTransfersWithRewardAndBalancesReplayFromGenesis()
    {
        var node = new TestNode();
        var wallet = node.NewWallet();
        node.Blockchain.MinePendingTransactions(wallet.Address);
        node.Blockchain.Submit(wallet.SignedTransfer("alice", 12.5m, "coffee"));

        Block block = node.Blockchain.MinePendingTransactions("validator-x");
        Assert.Equal(2, block.Transactions.Count); // transfer + reward
        Assert.Empty(node.Blockchain.PendingTransactions());
        Assert.True(block.HashIsConsistent());
        Assert.Equal("POS", block.ConsensusType);

        var balances = node.Blockchain.Balances();
        Assert.Equal(12.5m, balances["alice"]);
        Assert.Equal(50m - 12.5m, balances[wallet.Address]);
        Assert.Equal(50m, balances["validator-x"]);
        Assert.True(node.Blockchain.IsValid(), "full audit passes on an honestly built chain");
    }

    [Fact]
    public void TamperingWithAnyHistoricalTransactionInvalidatesTheChain()
    {
        var node = new TestNode();
        var wallet = node.NewWallet();
        node.Blockchain.MinePendingTransactions(wallet.Address);
        node.Blockchain.Submit(wallet.SignedTransfer("alice", 5m));
        node.Blockchain.MinePendingTransactions(wallet.Address);

        var tampered = node.Blockchain.Blocks().ToList();
        Block victim = tampered[2];
        // BE-01: bump the amount of the sealed transfer — Merkle root no longer matches.
        var txs = victim.Transactions.ToList();
        txs[0] = txs[0] with { Amount = 500m };
        tampered[2] = victim with { Transactions = txs };

        Assert.False(node.Blockchain.IsValidChain(tampered),
            "mutated transaction must break the Merkle root / block hash");
        Assert.True(node.Blockchain.IsValid(), "the real in-memory chain is untouched");
    }

    [Fact]
    public void ReplaceChainAdoptsOnlyStrictlyLongerFullyValidHistories()
    {
        var nodeA = new TestNode();
        var nodeB = new TestNode();
        nodeA.Blockchain.MinePendingTransactions("validator-a");
        nodeA.Blockchain.MinePendingTransactions("validator-a");
        nodeB.Blockchain.MinePendingTransactions("validator-b");

        // Same length or shorter → refused.
        Assert.False(nodeA.Blockchain.ReplaceChain(nodeB.Blockchain.Blocks()));

        // Longer and valid → adopted, then passes the adopter's own audit.
        Assert.True(nodeB.Blockchain.ReplaceChain(nodeA.Blockchain.Blocks()));
        Assert.Equal(3, nodeB.Blockchain.Length);
        Assert.True(nodeB.Blockchain.IsValid());

        // Longer but forged → refused.
        nodeA.Blockchain.MinePendingTransactions("validator-a");
        var forged = nodeA.Blockchain.Blocks().ToList();
        forged[1] = forged[1] with { ValidatorId = "mallory" }; // breaks the header hash
        Assert.False(nodeB.Blockchain.ReplaceChain(forged));
    }

    [Fact]
    public void RewardHalvesOnScheduleUnderTheSupplyCap()
    {
        var node = new TestNode();
        Assert.Equal(50m, node.Tokenomics.RewardForBlock(1, 0m));
        Assert.Equal(50m, node.Tokenomics.RewardForBlock(99, 0m));
        Assert.Equal(25m, node.Tokenomics.RewardForBlock(100, 0m));
        Assert.Equal(12.5m, node.Tokenomics.RewardForBlock(200, 0m));
        // Hard cap: issuance never exceeds what remains.
        Assert.Equal(3m, node.Tokenomics.RewardForBlock(1, 20997m));
        Assert.Equal(0m, node.Tokenomics.RewardForBlock(1, 21000m));
    }
}
