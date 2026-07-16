#include "contracts/ContractEngine.h"

#include "core/IdGen.h"

namespace legalchain::contracts {

ContractResult ContractEngine::execute(SmartContract& contract, core::TransactionType type,
                                        const std::map<std::string, std::string>& input) {
    ContractResult result = contract.execute(input);

    core::Transaction unsigned_(
        core::newUuid(), core::nowMillis(), wallet_.address(), contract.contractId(), 0.0, type,
        result.record, wallet_.publicKeyBase64(), std::nullopt);
    core::Transaction signed_(
        unsigned_.id(), unsigned_.timestampMs(), unsigned_.sender(), unsigned_.recipient(),
        unsigned_.amount(), unsigned_.type(), unsigned_.payload(), unsigned_.senderPublicKey(),
        wallet_.sign(unsigned_.contentToSign()));

    blockchain_.submit(signed_);
    return result.withTxId(signed_.id());
}

} // namespace legalchain::contracts
