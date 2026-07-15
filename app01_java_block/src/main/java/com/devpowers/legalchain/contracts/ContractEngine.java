package com.devpowers.legalchain.contracts;

import com.devpowers.legalchain.core.Blockchain;
import com.devpowers.legalchain.core.NodeWallet;
import com.devpowers.legalchain.core.Transaction;
import com.devpowers.legalchain.core.TransactionType;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.Map;
import java.util.UUID;

/**
 * Executes a {@link SmartContract} and anchors the result on-chain: the contract's
 * output record becomes the payload of a ledger transaction signed with this node's
 * ML-DSA wallet. The chain therefore holds a signed, timestamped, tamper-evident proof
 * of every contract execution — the "code is auditable law" property of Blockchain 3.0.
 */
@Service
public class ContractEngine {

    private final Blockchain blockchain;
    private final NodeWallet wallet;

    public ContractEngine(Blockchain blockchain, NodeWallet wallet) {
        this.blockchain = blockchain;
        this.wallet = wallet;
    }

    public ContractResult execute(SmartContract contract, TransactionType type,
                                  Map<String, String> input) {
        ContractResult result = contract.execute(input);

        Transaction unsigned = new Transaction(
                UUID.randomUUID().toString(),
                System.currentTimeMillis(),
                wallet.address(),
                contract.contractId(),
                BigDecimal.ZERO,
                type,
                result.record(),
                wallet.publicKeyBase64(),
                null);
        Transaction signed = new Transaction(
                unsigned.id(), unsigned.timestamp(), unsigned.sender(), unsigned.recipient(),
                unsigned.amount(), unsigned.type(), unsigned.payload(),
                unsigned.senderPublicKey(), wallet.sign(unsigned.contentToSign()));

        blockchain.submit(signed);
        return result.withTxId(signed.id());
    }
}
