#pragma once

#include <memory>
#include <vector>

#include "config/Config.h"
#include "consensus/BftStrategy.h"
#include "consensus/ConsensusEngine.h"
#include "consensus/ProofOfStakeStrategy.h"
#include "consensus/ProofOfWorkStrategy.h"
#include "contracts/AgriSupplyChainContract.h"
#include "contracts/ContractEngine.h"
#include "contracts/MedicalConsentContract.h"
#include "core/Blockchain.h"
#include "core/EventBus.h"
#include "core/NodeWallet.h"
#include "core/TokenomicsService.h"
#include "crypto/PqcKeyExchangeService.h"
#include "crypto/PqcSignatureService.h"
#include "crypto/SecureChannel.h"
#include "nft/NftService.h"
#include "p2p/P2pClientService.h"
#include "p2p/P2pService.h"

namespace legalchain::config {

/// Owns every service for one node process and wires their dependencies —
/// the C++ analogue of Spring's application context, built by hand since
/// there is no DI container. Member declaration order below IS the
/// construction order (C++ initializes members in declaration order,
/// regardless of the initializer-list order), so it must stay dependency-safe:
/// each service only refers to services declared above it.
class AppContext {
public:
    explicit AppContext(Config config)
        : config_(std::move(config)),
          events_(),
          signatureService_(),
          keyExchangeService_(),
          secureChannel_(),
          wallet_(signatureService_),
          consensus_(buildStrategies(), config_.consensusDefault),
          tokenomics_(config_.blockReward, config_.halvingInterval, config_.maxSupply),
          blockchain_(consensus_, tokenomics_, signatureService_, events_),
          contractEngine_(blockchain_, wallet_),
          nftService_(blockchain_, wallet_),
          p2pService_(blockchain_, wallet_, signatureService_, keyExchangeService_, secureChannel_, events_),
          p2pClient_(p2pService_) {}

    const Config& config() const { return config_; }
    core::EventBus& events() { return events_; }
    core::NodeWallet& wallet() { return wallet_; }
    consensus::ConsensusEngine& consensus() { return consensus_; }
    core::Blockchain& blockchain() { return blockchain_; }
    contracts::ContractEngine& contractEngine() { return contractEngine_; }
    contracts::MedicalConsentContract& medical() { return medical_; }
    contracts::AgriSupplyChainContract& agri() { return agri_; }
    nft::NftService& nftService() { return nftService_; }
    p2p::P2pService& p2pService() { return p2pService_; }
    p2p::P2pClientService& p2pClient() { return p2pClient_; }

private:
    Config config_;
    core::EventBus events_;
    crypto::PqcSignatureService signatureService_;
    crypto::PqcKeyExchangeService keyExchangeService_;
    crypto::SecureChannel secureChannel_;
    core::NodeWallet wallet_;
    consensus::ConsensusEngine consensus_;
    core::TokenomicsService tokenomics_;
    core::Blockchain blockchain_;
    contracts::ContractEngine contractEngine_;
    contracts::MedicalConsentContract medical_;
    contracts::AgriSupplyChainContract agri_;
    nft::NftService nftService_;
    p2p::P2pService p2pService_;
    p2p::P2pClientService p2pClient_;

    static std::vector<std::unique_ptr<consensus::ConsensusStrategy>> buildStrategies() {
        std::vector<std::unique_ptr<consensus::ConsensusStrategy>> strategies;
        strategies.push_back(std::make_unique<consensus::ProofOfWorkStrategy>());
        strategies.push_back(std::make_unique<consensus::ProofOfStakeStrategy>());
        strategies.push_back(std::make_unique<consensus::BftStrategy>());
        return strategies;
    }
};

} // namespace legalchain::config
