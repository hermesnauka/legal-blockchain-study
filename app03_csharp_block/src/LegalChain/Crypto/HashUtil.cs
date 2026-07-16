using System.Text;
using Org.BouncyCastle.Crypto.Digests;

namespace LegalChain.Crypto;

/// <summary>
/// SHA3-256 hashing utilities for block hashes and Merkle roots.
/// </summary>
/// <remarks>
/// Why SHA3-256: Grover's algorithm gives a quantum attacker at most a quadratic
/// speed-up against hash pre-images, so a 256-bit sponge construction (Keccak) retains
/// ~128-bit post-quantum security. Unlike the signature layer, the hash layer therefore
/// needs no algorithm swap to stay quantum-resistant — which is why Bitcoin's SHA-256
/// chaining is not its quantum weakness (its ECDSA signatures are).
/// </remarks>
public static class HashUtil
{
    /// <summary>SHA3-256 of the UTF-8 bytes of <paramref name="data"/>, as lowercase hex.</summary>
    public static string Sha3(string data)
    {
        var digest = new Sha3Digest(256);
        byte[] input = Encoding.UTF8.GetBytes(data);
        digest.BlockUpdate(input, 0, input.Length);
        byte[] output = new byte[digest.GetDigestSize()];
        digest.DoFinal(output, 0);
        return Convert.ToHexStringLower(output);
    }

    /// <summary>
    /// Computes a Merkle root over the given leaf hashes. The root commits to every
    /// transaction in a block: changing any transaction changes the root, which changes
    /// the block hash, which breaks every subsequent <c>previousHash</c> link — this is
    /// what makes the ledger tamper-evident and auditable (a compliance requirement for
    /// a general ledger).
    /// </summary>
    public static string MerkleRoot(IReadOnlyList<string> leaves)
    {
        if (leaves.Count == 0)
        {
            return Sha3("EMPTY");
        }
        var level = leaves.Select(Sha3).ToList();
        while (level.Count > 1)
        {
            var next = new List<string>(level.Count / 2 + 1);
            for (int i = 0; i < level.Count; i += 2)
            {
                string left = level[i];
                string right = (i + 1 < level.Count) ? level[i + 1] : left;
                next.Add(Sha3(left + right));
            }
            level = next;
        }
        return level[0];
    }
}
