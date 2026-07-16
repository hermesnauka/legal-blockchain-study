using LegalChain.Config;
using LegalChain.Consensus;
using LegalChain.Core;
using LegalChain.Crypto;
using Org.BouncyCastle.Crypto.Parameters;

namespace LegalChain.Tests;

/// <summary>
/// Hand-wired ledger object graph for unit tests — the same construction order as the
/// production DI registration in <c>LegalChainApp</c>, without a web host.
/// </summary>
internal class TestNode
{
    public LegalChainOptions Options { get; } = new();
    public EventBus Events { get; } = new();
    public PqcSignatureService Signatures { get; } = new();
    public ConsensusEngine Consensus { get; }
    public TokenomicsService Tokenomics { get; }
    public Blockchain Blockchain { get; }

    public TestNode(string defaultStrategy = "POS")
    {
        Consensus = new ConsensusEngine(
            [new ProofOfWorkStrategy(), new ProofOfStakeStrategy(), new BftStrategy()],
            defaultStrategy);
        Tokenomics = new TokenomicsService(Options);
        Blockchain = new Blockchain(Consensus, Tokenomics, Signatures, Events);
    }

    /// <summary>A user wallet for tests: fresh ML-DSA identity that can sign transfers.</summary>
    public TestWallet NewWallet() => new(Signatures);

    internal class TestWallet
    {
        private readonly PqcSignatureService _signatures;
        private readonly MLDsaPrivateKeyParameters _privateKey;

        public TestWallet(PqcSignatureService signatures)
        {
            _signatures = signatures;
            var keyPair = signatures.GenerateKeyPair();
            _privateKey = (MLDsaPrivateKeyParameters)keyPair.Private;
            PublicKeyBase64 = signatures.EncodePublicKey((MLDsaPublicKeyParameters)keyPair.Public);
            Address = signatures.Fingerprint((MLDsaPublicKeyParameters)keyPair.Public);
        }

        public string Address { get; }
        public string PublicKeyBase64 { get; }

        public Transaction SignedTransfer(string recipient, decimal amount, string memo = "")
        {
            var unsigned = new Transaction(
                Guid.NewGuid().ToString(),
                DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
                Address, recipient, amount, TransactionType.TRANSFER,
                new Dictionary<string, string> { ["memo"] = memo },
                PublicKeyBase64, null);
            return unsigned with { Signature = _signatures.Sign(unsigned.ContentToSign(), _privateKey) };
        }
    }
}
