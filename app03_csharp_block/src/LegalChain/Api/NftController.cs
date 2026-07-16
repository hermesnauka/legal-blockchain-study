using System.ComponentModel.DataAnnotations;
using LegalChain.Nft;
using Microsoft.AspNetCore.Mvc;
using NftToken = LegalChain.Nft.Nft;

namespace LegalChain.Api;

/// <summary>NFT minting and gallery: on-chain certification of digital ownership + SSI.</summary>
[ApiController]
[Route("/api/nft")]
public class NftController : ControllerBase
{
    private readonly NftService _nftService;

    public NftController(NftService nftService)
    {
        _nftService = nftService;
    }

    [HttpPost("mint")]
    public NftToken Mint([FromBody] MintRequest request)
        => _nftService.Mint(request.Title, request.Description,
            string.IsNullOrWhiteSpace(request.MetadataUri)
                ? "ipfs://placeholder"
                : request.MetadataUri);

    [HttpGet]
    public IReadOnlyList<NftToken> All() => _nftService.All();

    public record MintRequest(
        [Required(AllowEmptyStrings = false)] string Title,
        [Required(AllowEmptyStrings = false)] string Description,
        string? MetadataUri);
}
