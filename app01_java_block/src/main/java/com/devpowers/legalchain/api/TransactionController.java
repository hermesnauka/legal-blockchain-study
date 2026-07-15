package com.devpowers.legalchain.api;

import com.devpowers.legalchain.core.Blockchain;
import com.devpowers.legalchain.core.NodeWallet;
import com.devpowers.legalchain.core.Transaction;
import com.devpowers.legalchain.core.TransactionType;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import jakarta.validation.constraints.PositiveOrZero;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.math.BigDecimal;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Token transfers. In the MVP the browser has no key store, so the node wallet signs
 * on the user's behalf (custodial model); the ML-DSA signature is created here and
 * verified again by {@link Blockchain#submit} — the ledger never trusts the API layer.
 */
@RestController
@RequestMapping("/api/transactions")
public class TransactionController {

    private final Blockchain blockchain;
    private final NodeWallet wallet;

    public TransactionController(Blockchain blockchain, NodeWallet wallet) {
        this.blockchain = blockchain;
        this.wallet = wallet;
    }

    @GetMapping("/pending")
    public List<Transaction> pending() {
        return blockchain.pendingTransactions();
    }

    @PostMapping
    public Transaction submit(@Valid @RequestBody TransferRequest request) {
        Transaction unsigned = new Transaction(
                UUID.randomUUID().toString(),
                System.currentTimeMillis(),
                wallet.address(),
                request.recipient(),
                request.amount(),
                TransactionType.TRANSFER,
                request.memo() == null ? Map.of() : Map.of("memo", request.memo()),
                wallet.publicKeyBase64(),
                null);
        Transaction signed = new Transaction(
                unsigned.id(), unsigned.timestamp(), unsigned.sender(), unsigned.recipient(),
                unsigned.amount(), unsigned.type(), unsigned.payload(),
                unsigned.senderPublicKey(), wallet.sign(unsigned.contentToSign()));
        return blockchain.submit(signed);
    }

    public record TransferRequest(
            @NotBlank String recipient,
            @NotNull @PositiveOrZero BigDecimal amount,
            String memo) {
    }
}
