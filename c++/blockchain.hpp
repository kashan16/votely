#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <sstream>
#include <stdexcept>

// ─────────────────────────────────────────────
//  VoteData
//  What gets stored inside every block.
//  voter_hash  = SHA-256(user_id)   — pseudonymous
//  candidate   = candidate identifier
//  election_id = which election this vote belongs to
// ─────────────────────────────────────────────
struct VoteData {
    std::string voter_hash;
    std::string candidate_id;
    std::string election_id;
};

// ─────────────────────────────────────────────
//  Block
// ─────────────────────────────────────────────
struct Block {
    int         index;
    std::time_t timestamp;
    VoteData    vote;
    std::string prev_hash;
    std::string hash;
    int         nonce;

    // Serialize vote payload to a deterministic string for hashing
    std::string votePayload() const;

    // The string that gets hashed to produce this block's hash
    std::string toHashInput() const;

    // Compute SHA-256 of toHashInput()
    std::string computeHash() const;

    // Simple proof-of-work: find nonce so that hash starts with `difficulty` zeros
    void mine(int difficulty);

    // Convert block to a JSON string for storage
    std::string toJson() const;

    // Construct a Block from a JSON string (throws on parse error)
    static Block fromJson(const std::string& json);
};

// ─────────────────────────────────────────────
//  Blockchain
// ─────────────────────────────────────────────
class Blockchain {
public:
    explicit Blockchain(int difficulty = 2);

    // Add a vote — returns the new block's hash
    std::string addVote(const VoteData& vote);

    // Verify every block's hash and the prev_hash linkage
    bool isValid() const;

    // Count votes per candidate for a given election
    // Returns JSON: {"candidate_a": 3, "candidate_b": 5, ...}
    std::string tallyVotes(const std::string& election_id) const;

    // Serialize the whole chain to JSON array string
    std::string serialize() const;

    // Rebuild a Blockchain from a JSON array string (throws on error)
    static Blockchain deserialize(const std::string& json, int difficulty = 2);

    // Number of blocks (including genesis)
    int size() const { return static_cast<int>(chain_.size()); }

    // Get a block by index (throws if out of range)
    const Block& getBlock(int index) const;

    // Expose the raw difficulty for re-serialization
    int getDifficulty() const { return difficulty_; }

    // ── Merkle tree ──────────────────────────────────────────
    // Compute the Merkle root of all block hashes.
    // Pairs adjacent hashes and SHA-256s them together, level by level,
    // until a single root hash remains.
    // Returns a 64-char hex string (or 64 zeros for an empty chain).
    std::string merkleRoot() const;

private:
    int                difficulty_;
    std::vector<Block> chain_;

    Block createGenesis() const;
    const Block& lastBlock() const { return chain_.back(); }
};