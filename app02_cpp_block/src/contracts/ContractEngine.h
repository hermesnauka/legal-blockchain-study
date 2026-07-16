#pragma once

#include <map>
#include <string>

#include "contracts/ContractResult.h"
#include "contracts/SmartContract.h"
#include "core/Blockchain.h"
#include "core/NodeWallet.h"
#include "core/TransactionType.h"

namespace legalchain::contracts {

/// Executes a SmartContract and anchors the result on-chain: the contract's
/// output record becomes the payload of a ledger transaction signed with
/// this node's ML-DSA wallet. The chain therefore holds a signed,
/// timestamped, tamper-evident proof of every contract execution — the
/// "code is auditable law" property of Blockchain 3.0.
class ContractEngine {
public:
    ContractEngine(core::Blockchain& blockchain, core::NodeWallet& wallet)
        : blockchain_(blockchain), wallet_(wallet) {}

    ContractResult execute(SmartContract& contract, core::TransactionType type,
                            const std::map<std::string, std::string>& input);

private:
    core::Blockchain& blockchain_;
    core::NodeWallet& wallet_;
};

} // namespace legalchain::contracts
