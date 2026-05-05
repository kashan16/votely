/**
 * test.cpp  —  Native (non-WASM) test suite
 * Compile & run:
 *   g++ -std=c++17 -O2 blockchain.cpp test.cpp -o test && ./test
 */

#include "blockchain.hpp"
#include "picosha2.h"

#include <cassert>
#include <iostream>
#include <string>

#define GRN "\033[32m"
#define RED "\033[31m"
#define YEL "\033[33m"
#define RST "\033[0m"

static int passed = 0, failed = 0;

static void check(const std::string& label, bool condition) {
    if (condition) {
        std::cout << GRN << "  [PASS] " << RST << label << "\n";
        ++passed;
    } else {
        std::cout << RED << "  [FAIL] " << RST << label << "\n";
        ++failed;
    }
}

int main() {
    std::cout << YEL << "\n═══ Blockchain Voting — Unit Tests ═══\n\n" << RST;

    // ── 1. SHA-256 sanity ──────────────────────────────────
    std::cout << "▶ SHA-256\n";
    {
        std::string h = picosha2::hash256_hex_string("hello world");
        check("hash length is 64 hex chars", h.size() == 64);
        check("known vector matches",
              h == "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9");
        check("deterministic output", h == picosha2::hash256_hex_string("hello world"));
        check("different inputs differ",
              picosha2::hash256_hex_string("abc") != picosha2::hash256_hex_string("xyz"));
    }

    // ── 2. Genesis block ───────────────────────────────────
    std::cout << "\n▶ Genesis block\n";
    {
        Blockchain bc(2);
        check("chain starts with 1 block", bc.size() == 1);
        const Block& g = bc.getBlock(0);
        check("genesis index is 0",   g.index == 0);
        check("genesis prev_hash is 64 zeros", g.prev_hash == std::string(64, '0'));
        check("genesis vote tag is GENESIS",
              g.vote.election_id == "GENESIS" &&
              g.vote.voter_hash  == "GENESIS" &&
              g.vote.candidate_id == "GENESIS");
    }

    // ── 3. Adding votes ────────────────────────────────────
    std::cout << "\n▶ Adding votes\n";
    {
        Blockchain bc(2);
        std::string h1 = bc.addVote({"voter_hash_001", "alice", "election_2024"});
        std::string h2 = bc.addVote({"voter_hash_002", "bob",   "election_2024"});
        std::string h3 = bc.addVote({"voter_hash_003", "alice", "election_2024"});

        check("chain has 4 blocks after 3 votes", bc.size() == 4);
        check("returned hash is 64 chars", h1.size() == 64);
        check("all block hashes are unique", h1 != h2 && h2 != h3);
        check("block 1 meets difficulty", bc.getBlock(1).hash.substr(0,2) == "00");
        check("block 2 meets difficulty", bc.getBlock(2).hash.substr(0,2) == "00");
        check("block 3 meets difficulty", bc.getBlock(3).hash.substr(0,2) == "00");
    }

    // ── 4. Validation ──────────────────────────────────────
    std::cout << "\n▶ Chain validation\n";
    {
        Blockchain bc(2);
        bc.addVote({"h1", "alice", "e1"});
        bc.addVote({"h2", "bob",   "e1"});
        check("intact chain is valid", bc.isValid());

        std::string json = bc.serialize();
        size_t pos = json.find("\"candidate_id\":\"alice\"");
        if (pos != std::string::npos)
            json.replace(pos, std::string("\"candidate_id\":\"alice\"").size(),
                         "\"candidate_id\":\"ALICE\"");

        Blockchain tampered = Blockchain::deserialize(json, 2);
        check("tampered chain is invalid", !tampered.isValid());
    }

    // ── 5. Serialization round-trip ────────────────────────
    std::cout << "\n▶ Serialization round-trip\n";
    {
        Blockchain bc(2);
        bc.addVote({"hA", "alice", "election_X"});
        bc.addVote({"hB", "bob",   "election_X"});
        bc.addVote({"hC", "alice", "election_Y"});

        std::string json = bc.serialize();
        check("JSON is non-empty",      !json.empty());
        check("JSON starts with '['",   json.front() == '[');
        check("JSON ends with ']'",     json.back()  == ']');

        Blockchain bc2 = Blockchain::deserialize(json, 2);
        check("deserialized size matches", bc2.size() == bc.size());
        check("deserialized chain valid",  bc2.isValid());
        check("round-trip identical",      bc2.serialize() == json);
    }

    // ── 6. Vote tally ──────────────────────────────────────
    std::cout << "\n▶ Vote tallying\n";
    {
        Blockchain bc(2);
        bc.addVote({"h1", "alice", "election_2024"});
        bc.addVote({"h2", "bob",   "election_2024"});
        bc.addVote({"h3", "alice", "election_2024"});
        bc.addVote({"h4", "alice", "election_2024"});
        bc.addVote({"h5", "bob",   "other_election"});

        std::string tally = bc.tallyVotes("election_2024");
        std::cout << "    tally JSON: " << tally << "\n";
        check("tally contains alice",  tally.find("\"alice\"") != std::string::npos);
        check("tally contains bob",    tally.find("\"bob\"")   != std::string::npos);
        check("alice count is 3",      tally.find("\"alice\":3") != std::string::npos);
        check("bob count is 1",        tally.find("\"bob\":1")  != std::string::npos);

        std::string otherTally = bc.tallyVotes("other_election");
        check("other_election has bob",     otherTally.find("\"bob\":1") != std::string::npos);
        check("other_election has no alice",otherTally.find("alice") == std::string::npos);
    }

    // ── 7. Merkle tree ────────────────────────────────────
    std::cout << "\n▶ Merkle tree\n";
    {
        // Single-block chain (genesis only) → root == genesis hash
        Blockchain bc1(2);
        std::string root1 = bc1.merkleRoot();
        check("merkle root is 64 chars", root1.size() == 64);
        check("single-block root == genesis hash",
              root1 == bc1.getBlock(0).hash);

        // Two-block chain: root = SHA-256(genesis_hash || block1_hash)
        Blockchain bc2(2);
        bc2.addVote({"hX", "alice", "e1"});
        std::string root2 = bc2.merkleRoot();
        std::string expected2 = picosha2::hash256_hex_string(
            bc2.getBlock(0).hash + bc2.getBlock(1).hash);
        check("two-block root is correct", root2 == expected2);

        // Root is deterministic
        check("merkle root is deterministic", bc2.merkleRoot() == root2);

        // Tamper detection: serialize, mutate one block hash, rebuild
        std::string json = bc2.serialize();
        // Flip one character in a stored hash field
        size_t hashPos = json.find("\"hash\":\"00");
        if (hashPos != std::string::npos) {
            // Change one hex digit in the stored hash
            size_t valStart = hashPos + 8; // skip past  "hash":"
            json[valStart + 10] ^= 1;     // flip a bit in the middle
        }
        // Deserialize (this doesn't re-mine, so hashes stay tampered)
        Blockchain bc3 = Blockchain::deserialize(json, 2);
        check("tampered chain has different merkle root",
              bc3.merkleRoot() != root2);

        // Adding more votes changes root
        Blockchain bc4(2);
        bc4.addVote({"h1", "alice", "e1"});
        std::string rootA = bc4.merkleRoot();
        bc4.addVote({"h2", "bob", "e1"});
        std::string rootB = bc4.merkleRoot();
        check("adding a vote changes merkle root", rootA != rootB);

        // Odd-length chain: genesis + 2 votes = 3 blocks
        // The last hash is duplicated before hashing the final pair.
        Blockchain bc5(2);
        bc5.addVote({"hA", "alice", "e1"});
        bc5.addVote({"hB", "bob",   "e1"});
        std::string root5 = bc5.merkleRoot();
        check("odd-length chain produces 64-char root", root5.size() == 64);
        // Manually verify: level=[h0,h1,h2] → pad → [h0,h1,h2,h2]
        // → [SHA(h0||h1), SHA(h2||h2)] → SHA(those two)
        std::string h0 = bc5.getBlock(0).hash;
        std::string h1 = bc5.getBlock(1).hash;
        std::string h2 = bc5.getBlock(2).hash;
        std::string lv1_0 = picosha2::hash256_hex_string(h0 + h1);
        std::string lv1_1 = picosha2::hash256_hex_string(h2 + h2);
        std::string expected5 = picosha2::hash256_hex_string(lv1_0 + lv1_1);
        check("odd-length root matches manual computation", root5 == expected5);

        std::cout << "    merkle root (3 blocks): " << root5.substr(0,16) << "…\n";
    }

    // ── 8. Error handling ──────────────────────────────────
    std::cout << "\n▶ Error handling\n";
    {
        Blockchain bc(2);
        bool threw = false;
        try { bc.getBlock(99); } catch (const std::out_of_range&) { threw = true; }
        check("getBlock out-of-range throws", threw);

        bool threw2 = false;
        try { Blockchain::deserialize("not json at all", 2); }
        catch (...) { threw2 = true; }
        check("deserialize garbage throws", threw2);
    }

    // ─── Summary ──────────────────────────────────────────
    std::cout << "\n" << YEL << "══════════════════════════\n" << RST;
    std::cout << "  Passed: " << GRN << passed << RST << "\n";
    if (failed > 0)
        std::cout << "  Failed: " << RED << failed << RST << "\n";
    std::cout << YEL << "══════════════════════════\n\n" << RST;

    return failed > 0 ? 1 : 0;
}