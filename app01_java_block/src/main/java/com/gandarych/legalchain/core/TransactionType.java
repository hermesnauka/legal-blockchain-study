package com.gandarych.legalchain.core;

/** Ledger entry categories; contract types make sector use-cases auditable on-chain. */
public enum TransactionType {
    TRANSFER,
    REWARD,
    NFT_MINT,
    CONTRACT_MEDICAL,
    CONTRACT_AGRI
}
