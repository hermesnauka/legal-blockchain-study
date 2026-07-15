package com.devpowers.legalchain.nft;

/**
 * A minted non-fungible token: on-chain certificate of digital ownership.
 *
 * @param tokenId     unique token id (UUID)
 * @param title       work title
 * @param description work description
 * @param metadataUri pointer to off-chain metadata/asset (e.g. {@code ipfs://...}) —
 *                    the ledger stores only this URI and its provenance, never the asset
 * @param creator     ML-DSA key fingerprint of the creator (self-sovereign identity)
 * @param mintedAt    epoch millis of minting
 * @param txId        ledger transaction that certifies the mint
 */
public record Nft(
        String tokenId,
        String title,
        String description,
        String metadataUri,
        String creator,
        long mintedAt,
        String txId) {
}
