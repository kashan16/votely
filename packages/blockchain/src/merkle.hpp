#pragma once
#include <openssl/sha.h>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include "transaction.hpp"

inline std::string sha256hex(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)data.c_str(), data.size(), hash);
    std::stringstream ss;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i)
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    return ss.str();
}

inline std::string computeMerkleRoot(const std::vector<Transaction>& txs) {
    if (txs.empty()) return std::string(64, '0');

    std::vector<std::string> hashes;
    for (const auto& tx : txs)
        hashes.push_back(sha256hex(tx.voterID + tx.candidate + tx.electionID + tx.signature));

    while (hashes.size() > 1) {
        if (hashes.size() % 2 != 0)
            hashes.push_back(hashes.back()); // duplicate last if odd
        std::vector<std::string> next;
        for (size_t i = 0; i < hashes.size(); i += 2)
            next.push_back(sha256hex(hashes[i] + hashes[i + 1]));
        hashes = next;
    }
    return hashes[0];
}