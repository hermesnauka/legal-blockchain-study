package com.devpowers.legalchain;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;

/**
 * LegalChain — an educational, compliance-oriented Blockchain 3.0 general ledger (MVP).
 *
 * <p>Security model in one paragraph: every transaction is signed with ML-DSA
 * (NIST FIPS 204, CRYSTALS-Dilithium), every block is chained with SHA3-256 hashes
 * and a Merkle root, and the P2P channel between two remote nodes is protected by an
 * ML-KEM (NIST FIPS 203, CRYSTALS-Kyber) key encapsulation that derives a fresh
 * AES-256-GCM session key — a QKD-inspired design in which no long-lived symmetric
 * key ever exists and a quantum adversary recording traffic today ("harvest now,
 * decrypt later") still cannot recover the session key.</p>
 */
@SpringBootApplication
public class LegalChainApplication {

    public static void main(String[] args) {
        SpringApplication.run(LegalChainApplication.class, args);
    }
}
