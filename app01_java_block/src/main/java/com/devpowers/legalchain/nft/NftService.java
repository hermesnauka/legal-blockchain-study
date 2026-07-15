package com.devpowers.legalchain.nft;

import com.devpowers.legalchain.core.Blockchain;
import com.devpowers.legalchain.core.NodeWallet;
import com.devpowers.legalchain.core.Transaction;
import com.devpowers.legalchain.core.TransactionType;
import org.springframework.stereotype.Service;

import java.math.BigDecimal;
import java.util.List;
import java.util.Map;
import java.util.UUID;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * NFT minting: certifies digital ownership and creator identity (SSI) on the ledger.
 *
 * <p>How this certifies authorship without any registry or notary: the mint transaction
 * is signed with the creator's ML-DSA key, and the creator's address <i>is</i> the
 * fingerprint of that key. Anyone can verify that whoever controls that key minted this
 * token at this time — self-sovereign identity in its purest form. The asset itself
 * stays off-chain behind {@code metadataUri} (IPFS in production: a content-addressed
 * URI, so the metadata is itself tamper-evident), keeping the chain lean and GDPR-safe.</p>
 */
@Service
public class NftService {

    private final Blockchain blockchain;
    private final NodeWallet wallet;
    private final List<Nft> minted = new CopyOnWriteArrayList<>();

    public NftService(Blockchain blockchain, NodeWallet wallet) {
        this.blockchain = blockchain;
        this.wallet = wallet;
    }

    public Nft mint(String title, String description, String metadataUri) {
        String tokenId = UUID.randomUUID().toString();
        long now = System.currentTimeMillis();

        Transaction unsigned = new Transaction(
                UUID.randomUUID().toString(), now,
                wallet.address(), "nft-registry",
                BigDecimal.ZERO, TransactionType.NFT_MINT,
                Map.of("tokenId", tokenId, "title", title,
                        "description", description, "metadataUri", metadataUri),
                wallet.publicKeyBase64(), null);
        Transaction signed = new Transaction(
                unsigned.id(), unsigned.timestamp(), unsigned.sender(), unsigned.recipient(),
                unsigned.amount(), unsigned.type(), unsigned.payload(),
                unsigned.senderPublicKey(), wallet.sign(unsigned.contentToSign()));

        blockchain.submit(signed);

        Nft nft = new Nft(tokenId, title, description, metadataUri,
                wallet.address(), now, signed.id());
        minted.add(nft);
        return nft;
    }

    public List<Nft> all() {
        return List.copyOf(minted);
    }
}
