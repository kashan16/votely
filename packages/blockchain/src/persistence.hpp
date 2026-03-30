#pragma once
#include <fstream>
#include <vector>
#include <string>
#include <stdexcept>
#include "block.hpp"
#include "json.hpp"

using json = nlohmann::json;

inline json txToJson(const Transaction& tx) {
    return {
        {"voterID",    tx.voterID},
        {"candidate",  tx.candidate},
        {"electionID", tx.electionID},
        {"signature",  tx.signature},   // base64-encode in production
        {"timestamp",  tx.timestamp}
    };
}

inline Transaction txFromJson(const json& j) {
    return {
        j["voterID"],
        j["candidate"],
        j["electionID"],
        j["signature"],
        j["timestamp"]
    };
}

inline void saveChain(const std::vector<Block>& chain, const std::string& path) {
    json j = json::array();
    for (const auto& b : chain) {
        json txs = json::array();
        for (const auto& tx : b.transactions) txs.push_back(txToJson(tx));
        j.push_back({
            {"index",        b.index},
            {"previousHash", b.previousHash},
            {"merkleRoot",   b.merkleRoot},
            {"hash",         b.hash},
            {"timestamp",    (long long)b.timestamp},
            {"nonce",        b.nonce},
            {"transactions", txs}
        });
    }
    std::ofstream f(path);
    if (!f) throw std::runtime_error("Cannot open chain file: " + path);
    f << j.dump(2);
    std::cout << "[persist] saved " << chain.size() << " blocks → " << path << "\n";
}

inline std::vector<Block> loadChain(const std::string& path) {
    std::ifstream f(path);
    if (!f) return {};   // first run — empty chain is fine
    json j;
    f >> j;
    std::vector<Block> chain;
    for (const auto& jb : j) {
        Block b;
        b.index        = jb["index"];
        b.previousHash = jb["previousHash"];
        b.merkleRoot   = jb["merkleRoot"];
        b.hash         = jb["hash"];
        b.timestamp    = jb["timestamp"];
        b.nonce        = jb["nonce"];
        for (const auto& jtx : jb["transactions"])
            b.transactions.push_back(txFromJson(jtx));
        chain.push_back(b);
    }
    std::cout << "[persist] loaded " << chain.size() << " blocks ← " << path << "\n";
    return chain;
}