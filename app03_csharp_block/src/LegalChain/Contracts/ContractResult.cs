namespace LegalChain.Contracts;

/// <summary>
/// Outcome of a smart-contract execution: the on-chain record (payload) plus a
/// human-readable message for the UI.
/// </summary>
/// <param name="ContractId">which contract produced this result</param>
/// <param name="Accepted">whether the state transition was applied</param>
/// <param name="Message">human-readable outcome (shown in the frontend)</param>
/// <param name="Record">the exact key/value data recorded in the ledger transaction</param>
/// <param name="TxId">id of the ledger transaction recording this execution (set by the engine)</param>
public record ContractResult(
    string ContractId,
    bool Accepted,
    string Message,
    IReadOnlyDictionary<string, string> Record,
    string? TxId)
{
    public ContractResult WithTxId(string txId)
        => new(ContractId, Accepted, Message, Record, txId);
}
