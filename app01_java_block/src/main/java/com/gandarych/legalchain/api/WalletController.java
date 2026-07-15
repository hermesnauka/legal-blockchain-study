package com.gandarych.legalchain.api;

import com.gandarych.legalchain.core.Blockchain;
import com.gandarych.legalchain.core.NodeWallet;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.math.BigDecimal;
import java.util.Map;

/** This node's post-quantum wallet (self-sovereign identity) and the derived balances. */
@RestController
@RequestMapping("/api/wallet")
public class WalletController {

    private final NodeWallet wallet;
    private final Blockchain blockchain;

    public WalletController(NodeWallet wallet, Blockchain blockchain) {
        this.wallet = wallet;
        this.blockchain = blockchain;
    }

    @GetMapping
    public Map<String, Object> wallet() {
        return Map.of(
                "address", wallet.address(),
                "algorithm", wallet.algorithm(),
                "publicKey", wallet.publicKeyBase64(),
                "fingerprint", wallet.address(),
                "balance", blockchain.balances().getOrDefault(wallet.address(), BigDecimal.ZERO));
    }

    /** All balances, derived by replaying the ledger — never stored, always auditable. */
    @GetMapping("/balances")
    public Map<String, BigDecimal> balances() {
        return blockchain.balances();
    }
}
