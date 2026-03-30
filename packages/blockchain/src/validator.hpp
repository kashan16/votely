#pragma once
#include <vector>
#include <iostream>
#include "block.hpp"
#include "crypto_utils.hpp"

// Full chain audit — run on startup after loading from disk,
// and optionally on each new block addition.
inline bool validateChain(const std::vector<Block>& chain) {
    for (size_t i = 1; i < chain.size(); ++i) {
        const Block& prev = chain[i - 1];
        const Block& curr = chain[i];

        // 1. Hash linkage
        if (curr.previousHash != prev.hash) {
            std::cerr << "[validator] block #" << i << " broken hash link\n";
            return false;
        }

        // 2. Hash integrity (recompute and compare)
        if (curr.hash != curr.computeHash()) {
            std::cerr << "[validator] block #" << i << " hash mismatch (tampered?)\n";
            return false;
        }

        // 3. Merkle root integrity
        if (curr.merkleRoot != computeMerkleRoot(curr.transactions)) {
            std::cerr << "[validator] block #" << i << " merkle root mismatch\n";
            return false;
        }
    }
    std::cout << "[validator] chain OK — " << chain.size() << " blocks\n";
    return true;
}