#pragma once
#include <string>
#include <iostream>
#include "block.hpp"

// Proof-of-Work: find nonce so hash starts with `difficulty` zeros.
// Difficulty 3 = ~4000 iterations on avg (fast for MVP).
// Raise to 4-5 for production hardening.
inline void mineBlock(Block& block, int difficulty) {
    const std::string target(difficulty, '0');
    block.nonce = 0;
    block.merkleRoot = computeMerkleRoot(block.transactions);
    do {
        ++block.nonce;
        block.hash = block.computeHash();
    } while (block.hash.substr(0, difficulty) != target);
    std::cout << "[miner] block #" << block.index
              << " mined  nonce=" << block.nonce
              << "  hash=" << block.hash.substr(0, 16) << "...\n";
}