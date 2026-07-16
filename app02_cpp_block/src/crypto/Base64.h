#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace legalchain::crypto {

/// Minimal, dependency-free Base64 codec (standard alphabet, padded) used to
/// carry raw key/ciphertext/signature bytes over JSON, matching the encoding
/// Java's java.util.Base64 produces.
class Base64 {
public:
    static std::string encode(const std::vector<uint8_t>& data) {
        static const char* table =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((data.size() + 2) / 3) * 4);
        std::size_t i = 0;
        while (i + 3 <= data.size()) {
            uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
            out += table[(n >> 18) & 0x3F];
            out += table[(n >> 12) & 0x3F];
            out += table[(n >> 6) & 0x3F];
            out += table[n & 0x3F];
            i += 3;
        }
        std::size_t remaining = data.size() - i;
        if (remaining == 1) {
            uint32_t n = data[i] << 16;
            out += table[(n >> 18) & 0x3F];
            out += table[(n >> 12) & 0x3F];
            out += "==";
        } else if (remaining == 2) {
            uint32_t n = (data[i] << 16) | (data[i + 1] << 8);
            out += table[(n >> 18) & 0x3F];
            out += table[(n >> 12) & 0x3F];
            out += table[(n >> 6) & 0x3F];
            out += '=';
        }
        return out;
    }

    static std::vector<uint8_t> decode(const std::string& input) {
        auto value = [](char c) -> int {
            if (c >= 'A' && c <= 'Z') return c - 'A';
            if (c >= 'a' && c <= 'z') return c - 'a' + 26;
            if (c >= '0' && c <= '9') return c - '0' + 52;
            if (c == '+') return 62;
            if (c == '/') return 63;
            return -1;
        };
        std::vector<uint8_t> out;
        out.reserve((input.size() / 4) * 3);
        int buffer = 0;
        int bits = 0;
        for (char c : input) {
            if (c == '=' || c == '\0') break;
            int v = value(c);
            if (v < 0) continue;
            buffer = (buffer << 6) | v;
            bits += 6;
            if (bits >= 8) {
                bits -= 8;
                out.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
            }
        }
        return out;
    }

    static std::string decodeToString(const std::string& input) {
        auto bytes = decode(input);
        return std::string(bytes.begin(), bytes.end());
    }
};

} // namespace legalchain::crypto
