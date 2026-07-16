using LegalChain.Core;

namespace LegalChain.Contracts;

/// <summary>
/// Executes an <see cref="ISmartContract"/> and anchors the result on-chain: the
/// contract's output record becomes the payload of a ledger transaction signed with this
/// node's ML-DSA wallet. The chain therefore holds a signed, timestamped, tamper-evident
/// proof of every contract execution — the "code is auditable law" property of
/// Blockchain 3.0.
/// </summary>
public class ContractEngine
{
    private readonly Blockchain _blockchain;
    private readonly NodeWallet _wallet;

    public ContractEngine(Blockchain blockchain, NodeWallet wallet)
    {
        _blockchain = blockchain;
        _wallet = wallet;
    }

    public ContractResult Execute(ISmartContract contract, TransactionType type,
        IReadOnlyDictionary<string, string> input)
    {
        ContractResult result = contract.Execute(input);

        var unsigned = new Transaction(
            Guid.NewGuid().ToString(),
            DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            _wallet.Address,
            contract.ContractId,
            0m,
            type,
            result.Record,
            _wallet.PublicKeyBase64,
            null);
        Transaction signed = unsigned with { Signature = _wallet.Sign(unsigned.ContentToSign()) };

        _blockchain.Submit(signed);
        return result.WithTxId(signed.Id);
    }
}
