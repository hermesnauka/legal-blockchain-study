package com.devpowers.legalchain.api;

import com.devpowers.legalchain.core.Block;
import com.devpowers.legalchain.core.Blockchain;
import com.devpowers.legalchain.core.NodeWallet;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;
import java.util.Map;

/** The general ledger view: read the chain, audit it, and seal new blocks. */
@RestController
@RequestMapping("/api/chain")
public class ChainController {

    private final Blockchain blockchain;
    private final NodeWallet wallet;

    public ChainController(Blockchain blockchain, NodeWallet wallet) {
        this.blockchain = blockchain;
        this.wallet = wallet;
    }

    @GetMapping
    public Map<String, Object> chain() {
        List<Block> blocks = blockchain.blocks();
        return Map.of(
                "length", blocks.size(),
                "valid", blockchain.isValid(),
                "blocks", blocks);
    }

    /** Full independent audit: hashes, links, Merkle roots, signatures, consensus proofs. */
    @GetMapping("/validate")
    public Map<String, Object> validate() {
        boolean valid = blockchain.isValid();
        return Map.of(
                "valid", valid,
                "message", valid
                        ? "Chain audit passed: all hashes, links, ML-DSA signatures and consensus proofs verified."
                        : "Chain audit FAILED: at least one block or signature does not verify.");
    }

    /** Seals pending transactions into a block using the active consensus strategy. */
    @PostMapping("/mine")
    public Block mine(@RequestBody(required = false) MineRequest request) {
        String validator = (request != null && request.validatorId() != null
                && !request.validatorId().isBlank())
                ? request.validatorId()
                : wallet.address();
        return blockchain.minePendingTransactions(validator);
    }

    public record MineRequest(String validatorId) {
    }
}
