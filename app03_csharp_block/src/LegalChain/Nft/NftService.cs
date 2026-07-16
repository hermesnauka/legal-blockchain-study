using LegalChain.Core;

namespace LegalChain.Nft;

/// <summary>
/// NFT minting: certifies digital ownership and creator identity (SSI) on the ledger.
/// </summary>
/// <remarks>
/// How this certifies authorship without any registry or notary: the mint transaction
/// is signed with the creator's ML-DSA key, and the creator's address <i>is</i> the
/// fingerprint of that key. Anyone can verify that whoever controls that key minted this
/// token at this time — self-sovereign identity in its purest form. The asset itself
/// stays off-chain behind <c>metadataUri</c> (IPFS in production: a content-addressed
/// URI, so the metadata is itself tamper-evident), keeping the chain lean and GDPR-safe.
/// </remarks>
public class NftService
{
    private readonly Blockchain _blockchain;
    private readonly NodeWallet _wallet;
    private readonly List<Nft> _minted = new();
    private readonly Lock _gate = new();

    public NftService(Blockchain blockchain, NodeWallet wallet)
    {
        _blockchain = blockchain;
        _wallet = wallet;
    }

    public Nft Mint(string title, string description, string metadataUri)
    {
        string tokenId = Guid.NewGuid().ToString();
        long now = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds();

        var unsigned = new Transaction(
            Guid.NewGuid().ToString(), now,
            _wallet.Address, "nft-registry",
            0m, TransactionType.NFT_MINT,
            new Dictionary<string, string>
            {
                ["tokenId"] = tokenId,
                ["title"] = title,
                ["description"] = description,
                ["metadataUri"] = metadataUri
            },
            _wallet.PublicKeyBase64, null);
        Transaction signed = unsigned with { Signature = _wallet.Sign(unsigned.ContentToSign()) };

        _blockchain.Submit(signed);

        var nft = new Nft(tokenId, title, description, metadataUri,
            _wallet.Address, now, signed.Id);
        lock (_gate)
        {
            _minted.Add(nft);
        }
        return nft;
    }

    public IReadOnlyList<Nft> All()
    {
        lock (_gate)
        {
            return _minted.ToList();
        }
    }
}
