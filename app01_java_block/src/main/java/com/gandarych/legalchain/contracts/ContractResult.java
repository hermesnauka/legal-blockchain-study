package com.gandarych.legalchain.contracts;

import java.util.Map;

/**
 * Outcome of a smart-contract execution: the on-chain record (payload) plus a
 * human-readable message for the UI.
 *
 * @param contractId which contract produced this result
 * @param accepted   whether the state transition was applied
 * @param message    human-readable outcome (shown in the frontend)
 * @param record     the exact key/value data recorded in the ledger transaction
 * @param txId       id of the ledger transaction recording this execution (set by the engine)
 */
public record ContractResult(
        String contractId,
        boolean accepted,
        String message,
        Map<String, String> record,
        String txId) {

    public ContractResult withTxId(String txId) {
        return new ContractResult(contractId, accepted, message, record, txId);
    }
}
