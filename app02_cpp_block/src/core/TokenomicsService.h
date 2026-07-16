#pragma once

#include <cstdint>
#include <vector>

#include "core/Block.h"
#include "core/Transaction.h"

namespace legalchain::core {

/// Educational tokenomics: a fixed block reward that halves every N blocks
/// under a hard supply cap (the Bitcoin issuance model, scaled down so
/// halvings are observable in a classroom session).
///
/// Why this matters for compliance: the total supply and the issuance
/// schedule are deterministic functions of the chain itself — no
/// discretionary "printing". An auditor can recompute the entire monetary
/// base from the ledger, which is exactly the transparency property
/// regulators (e.g. under MiCA) expect from a token issuer.
class TokenomicsService {
public:
    TokenomicsService(double blockReward, int64_t halvingInterval, double maxSupply)
        : blockReward_(blockReward), halvingInterval_(halvingInterval), maxSupply_(maxSupply) {}

    /// Reward for the block at blockIndex, after halvings and the supply cap.
    double rewardForBlock(int64_t blockIndex, double supplyAlreadyIssued) const;

    /// Coinbase-style reward transaction crediting the sealing validator.
    Transaction rewardTransaction(const std::string& validatorAddress, int64_t blockIndex,
                                   double supplyAlreadyIssued) const;

    /// Total supply issued so far = sum of all REWARD transactions in the chain.
    static double issuedSupply(const std::vector<Block>& chain);

private:
    double blockReward_;
    int64_t halvingInterval_;
    double maxSupply_;
};

} // namespace legalchain::core
