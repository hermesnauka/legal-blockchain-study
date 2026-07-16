namespace LegalChain.Nft;

/// <summary>
/// A minted non-fungible token: on-chain certificate of digital ownership.
/// </summary>
/// <param name="TokenId">unique token id (UUID)</param>
/// <param name="Title">work title</param>
/// <param name="Description">work description</param>
/// <param name="MetadataUri">pointer to off-chain metadata/asset (e.g. <c>ipfs://...</c>) —
/// the ledger stores only this URI and its provenance, never the asset</param>
/// <param name="Creator">ML-DSA key fingerprint of the creator (self-sovereign identity)</param>
/// <param name="MintedAt">epoch millis of minting</param>
/// <param name="TxId">ledger transaction that certifies the mint</param>
public record Nft(
    string TokenId,
    string Title,
    string Description,
    string MetadataUri,
    string Creator,
    long MintedAt,
    string TxId);
