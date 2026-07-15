package com.gandarych.legalchain.api;

import com.gandarych.legalchain.nft.Nft;
import com.gandarych.legalchain.nft.NftService;
import jakarta.validation.Valid;
import jakarta.validation.constraints.NotBlank;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import java.util.List;

/** NFT minting and gallery: on-chain certification of digital ownership + SSI. */
@RestController
@RequestMapping("/api/nft")
public class NftController {

    private final NftService nftService;

    public NftController(NftService nftService) {
        this.nftService = nftService;
    }

    @PostMapping("/mint")
    public Nft mint(@Valid @RequestBody MintRequest request) {
        return nftService.mint(request.title(), request.description(),
                request.metadataUri() == null || request.metadataUri().isBlank()
                        ? "ipfs://placeholder"
                        : request.metadataUri());
    }

    @GetMapping
    public List<Nft> all() {
        return nftService.all();
    }

    public record MintRequest(
            @NotBlank String title,
            @NotBlank String description,
            String metadataUri) {
    }
}
