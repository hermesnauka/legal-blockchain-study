#include "core/Blockchain.h"

#include <string>

#include "core/IdGen.h"

namespace legalchain::core {

const Block& Blockchain::genesis() {
    static const Block GENESIS = [] {
        std::string zeros(64, '0');
        std::vector<Transaction> noTx;
        std::string merkle = Block::merkleRootOf(noTx);
        std::string hash = Block::computeHash(0, 0, zeros, merkle, "GENESIS", "GENESIS", "genesis", 0);
        return Block(0, 0, zeros, merkle, noTx, "GENESIS", "GENESIS", "genesis", 0, hash);
    }();
    return GENESIS;
}

Blockchain::Blockchain(consensus::ConsensusEngine& consensusEngine, TokenomicsService tokenomics,
                        crypto::PqcSignatureService signatures, EventBus& events)
    : chain_{genesis()},
      consensusEngine_(consensusEngine),
      tokenomics_(std::move(tokenomics)),
      signatures_(std::move(signatures)),
      events_(events) {}

double Blockchain::pendingOutgoingOf(const std::string& sender) const {
    // Caller must already hold chainMutex_.
    double sum = 0.0;
    for (const auto& tx : pending_) {
        if (!tx.isReward() && tx.sender() == sender) {
            sum += tx.amount();
        }
    }
    return sum;
}

namespace {
std::map<std::string, double> balancesOf(const std::vector<Block>& chain) {
    std::map<std::string, double> result;
    for (const auto& block : chain) {
        for (const auto& tx : block.transactions()) {
            if (!tx.isReward()) {
                result[tx.sender()] -= tx.amount();
            }
            result[tx.recipient()] += tx.amount();
        }
    }
    return result;
}
} // namespace

Transaction Blockchain::submit(const Transaction& tx) {
    if (!tx.isReward()) {
        if (!tx.senderPublicKey() || !tx.signature()) {
            throw std::invalid_argument("Transaction must be signed (ML-DSA)");
        }
        if (!signatures_.verify(tx.contentToSign(), *tx.signature(), *tx.senderPublicKey())) {
            throw std::invalid_argument("Invalid ML-DSA signature");
        }
        auto publicKeyBytes = signatures_.decodePublicKey(*tx.senderPublicKey());
        std::string expectedSender = crypto::PqcSignatureService::fingerprint(publicKeyBytes);
        if (expectedSender != tx.sender()) {
            throw std::invalid_argument("Sender address does not match signing key");
        }
    }

    {
        std::lock_guard<std::mutex> lock(chainMutex_);
        if (!tx.isReward()) {
            // Balances computed from chain_ directly — chainMutex_ is already
            // held here, so calling the public balances()/blocks() (which
            // re-lock) would deadlock on this non-recursive mutex.
            auto bal = balancesOf(chain_);
            double available = (bal.count(tx.sender()) ? bal[tx.sender()] : 0.0) - pendingOutgoingOf(tx.sender());
            if (tx.amount() > available) {
                throw std::invalid_argument(
                    "Insufficient funds: sender " + tx.sender() + " has " + Transaction::formatAmount(available)
                    + " LGC available but tried to send " + Transaction::formatAmount(tx.amount()) + " LGC");
            }
        }
        pending_.push_back(tx);
    }
    Json::Value data = tx.toJson();
    events_.publish(LedgerEvent{LedgerEventType::TX_ADDED, data});
    return tx;
}

Block Blockchain::minePendingTransactions(const std::string& validatorAddress) {
    Block sealed;
    {
        std::lock_guard<std::mutex> lock(chainMutex_);
        const Block& previous = chain_.back();
        int64_t index = previous.index() + 1;

        std::vector<Transaction> blockTxs = pending_;
        blockTxs.push_back(
            tokenomics_.rewardTransaction(validatorAddress, index, TokenomicsService::issuedSupply(chain_)));

        BlockCandidate candidate(index, nowMillis(), previous.hash(), blockTxs, validatorAddress);
        sealed = consensusEngine_.active().seal(candidate);

        chain_.push_back(sealed);
        pending_.clear();
    }
    events_.publish(LedgerEvent{LedgerEventType::BLOCK_ADDED, sealed.toJson()});
    return sealed;
}

bool Blockchain::isValidChain(const std::vector<Block>& candidate) const {
    if (candidate.empty() || !(candidate.front() == genesis())) {
        return false;
    }
    for (std::size_t i = 1; i < candidate.size(); ++i) {
        const Block& block = candidate[i];
        const Block& previous = candidate[i - 1];
        if (block.index() != previous.index() + 1 || block.previousHash() != previous.hash()
            || !block.hashIsConsistent()) {
            return false;
        }
        try {
            if (!consensusEngine_.byName(block.consensusType()).validate(block)) {
                return false;
            }
        } catch (const std::invalid_argument&) {
            return false;
        }
        for (const auto& tx : block.transactions()) {
            if (!tx.isReward()
                && (!tx.signature() || !tx.senderPublicKey()
                    || !signatures_.verify(tx.contentToSign(), *tx.signature(), *tx.senderPublicKey()))) {
                return false;
            }
        }
    }
    return true;
}

bool Blockchain::isValid() const {
    return isValidChain(blocks());
}

bool Blockchain::replaceChain(const std::vector<Block>& candidate) {
    bool adopted = false;
    std::size_t candidateSize = candidate.size();
    {
        std::lock_guard<std::mutex> lock(chainMutex_);
        if (candidate.size() > chain_.size() && isValidChain(candidate)) {
            chain_ = candidate;
            pending_.clear();
            adopted = true;
        }
    }
    if (adopted) {
        Json::Value data(Json::objectValue);
        data["length"] = static_cast<Json::UInt64>(candidateSize);
        events_.publish(LedgerEvent{LedgerEventType::CHAIN_REPLACED, data});
    }
    return adopted;
}

std::map<std::string, double> Blockchain::balances() const {
    std::lock_guard<std::mutex> lock(chainMutex_);
    return balancesOf(chain_);
}

std::vector<Block> Blockchain::blocks() const {
    std::lock_guard<std::mutex> lock(chainMutex_);
    return chain_;
}

std::vector<Transaction> Blockchain::pendingTransactions() const {
    std::lock_guard<std::mutex> lock(chainMutex_);
    return pending_;
}

int Blockchain::length() const {
    std::lock_guard<std::mutex> lock(chainMutex_);
    return static_cast<int>(chain_.size());
}

} // namespace legalchain::core
