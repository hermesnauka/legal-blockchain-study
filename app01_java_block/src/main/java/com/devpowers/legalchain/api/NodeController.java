package com.devpowers.legalchain.api;

import com.devpowers.legalchain.consensus.ConsensusEngine;
import com.devpowers.legalchain.core.Blockchain;
import com.devpowers.legalchain.core.NodeWallet;
import com.devpowers.legalchain.p2p.P2pClientService;
import com.devpowers.legalchain.p2p.P2pService;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RestController;

import java.util.Map;

/** Node identity and P2P operations (connect to a peer, trigger a full ledger sync). */
@RestController
public class NodeController {

    private final NodeWallet wallet;
    private final Blockchain blockchain;
    private final ConsensusEngine consensus;
    private final P2pService p2p;
    private final P2pClientService p2pClient;
    private final String nodeName;
    private final int port;

    public NodeController(NodeWallet wallet, Blockchain blockchain, ConsensusEngine consensus,
                          P2pService p2p, P2pClientService p2pClient,
                          @Value("${legalchain.node.name:node}") String nodeName,
                          @Value("${server.port:8080}") int port) {
        this.wallet = wallet;
        this.blockchain = blockchain;
        this.consensus = consensus;
        this.p2p = p2p;
        this.p2pClient = p2pClient;
        this.nodeName = nodeName;
        this.port = port;
    }

    @GetMapping("/api/node")
    public Map<String, Object> node() {
        return Map.of(
                "nodeId", wallet.address(),
                "name", nodeName,
                "port", port,
                "consensus", consensus.active().name(),
                "peers", p2p.connectedPeers(),
                "chainLength", blockchain.length());
    }

    /** Opens a PQC-secured link to a remote peer, e.g. {@code ws://localhost:8081/ws/p2p}. */
    @PostMapping("/api/p2p/connect")
    public Map<String, Object> connect(@Valid @RequestBody ConnectRequest request) {
        p2pClient.connect(request.url());
        return Map.of("connected", true, "peer", request.url());
    }

    /** Requests the ledger from all established peers; longest valid chain wins locally. */
    @PostMapping("/api/p2p/sync")
    public Map<String, Object> sync() {
        int asked = p2p.syncWithPeers();
        return Map.of(
                "result", asked > 0
                        ? "Sync requested from " + asked + " peer(s)"
                        : "No established peers to sync with",
                "chainLength", blockchain.length());
    }

    public record ConnectRequest(@NotBlank String url) {
    }
}
