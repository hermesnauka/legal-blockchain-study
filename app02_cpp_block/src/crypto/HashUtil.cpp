#include "crypto/HashUtil.h"

#include <openssl/evp.h>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace legalchain::crypto {

std::string HashUtil::sha3(const std::string& data) {
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if (ctx == nullptr) {
        throw std::runtime_error("Cannot allocate EVP_MD_CTX");
    }
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    bool ok = EVP_DigestInit_ex(ctx, EVP_sha3_256(), nullptr) == 1
              && EVP_DigestUpdate(ctx, data.data(), data.size()) == 1
              && EVP_DigestFinal_ex(ctx, digest, &digestLen) == 1;
    EVP_MD_CTX_free(ctx);
    if (!ok) {
        throw std::runtime_error("SHA3-256 not available");
    }
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < digestLen; ++i) {
        oss << std::setw(2) << static_cast<int>(digest[i]);
    }
    return oss.str();
}

std::string HashUtil::merkleRoot(const std::vector<std::string>& leaves) {
    if (leaves.empty()) {
        return sha3("EMPTY");
    }
    std::vector<std::string> level;
    level.reserve(leaves.size());
    for (const auto& leaf : leaves) {
        level.push_back(sha3(leaf));
    }
    while (level.size() > 1) {
        std::vector<std::string> next;
        next.reserve(level.size() / 2 + 1);
        for (std::size_t i = 0; i < level.size(); i += 2) {
            const std::string& left = level[i];
            const std::string& right = (i + 1 < level.size()) ? level[i + 1] : left;
            next.push_back(sha3(left + right));
        }
        level = std::move(next);
    }
    return level.front();
}

} // namespace legalchain::crypto
