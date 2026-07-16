#pragma once

#include <map>
#include <string>

#include "contracts/ContractResult.h"

namespace legalchain::contracts {

/// Blockchain 3.0 smart contract abstraction: deterministic business logic
/// whose every execution is recorded as a signed ledger transaction.
///
/// Design rule for compliance (GDPR art. 17 "right to erasure" vs.
/// immutability): contracts must keep personal data off-chain and record
/// on-chain only hashes, pseudonymous identifiers and consent/state
/// transitions. The chain then proves *that* and *when* something happened
/// without ever containing erasable personal data — the standard pattern
/// that reconciles blockchains with GDPR.
class SmartContract {
public:
    virtual ~SmartContract() = default;

    /// Stable contract identifier recorded in the transaction payload.
    virtual std::string contractId() const = 0;

    /// Sector / purpose description surfaced by the educational frontend.
    virtual std::string description() const = 0;

    /// Validates the input against the contract's rules and returns the
    /// state transition to be recorded on-chain. Implementations must be
    /// deterministic and side-effect free apart from their own state
    /// registry (same input -> same result on every node).
    /// Throws std::invalid_argument if the input violates contract rules.
    virtual ContractResult execute(const std::map<std::string, std::string>& input) = 0;
};

} // namespace legalchain::contracts
