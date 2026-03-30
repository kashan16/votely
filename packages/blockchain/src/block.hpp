#pragma once
#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include "transaction.hpp"
#include "merkle.hpp"

struct Block {
    int index;
    std::vector<Transaction> transactions;
    std::string previousHash;
    std::string merkleRoot;
    std::string hash;
    time_t timestamp;
    int nonce;

    std::string computeHash() const {
        std::stringstream ss;
        ss << index << previousHash << timestamp << nonce << merkleRoot;
        return sha256hex(ss.str());
    }

    void finalise() {
        merkleRoot = computeMerkleRoot(transactions);
        hash = computeHash();
    }
};