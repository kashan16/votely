#include <iostream>
#include <string>
#include <unordered_map>
#include "blockchain.hpp"
#include "crypto_utils.hpp"

int main() {
    // difficulty=3 for fast MVP testing; raise to 4-5 for production
    Blockchain bc(3, "chain.json");

    // --- Register voters (in production: pulled from Supabase) ---
    struct Voter { std::string pub, priv; };
    std::unordered_map<std::string, Voter> voters;

    for (const std::string& id : {"Alice", "Bob", "Carol"}) {
        Voter v;
        generateKeyPair(v.pub, v.priv);
        voters[id] = v;
        std::cout << "[reg] voter registered: " << id << "\n";
    }

    const std::string ELECTION = "election-2025-general";

    // --- Cast votes ---
    bc.addVote("Alice", "CandidateA", ELECTION, voters["Alice"].priv, voters["Alice"].pub);
    bc.addVote("Bob",   "CandidateB", ELECTION, voters["Bob"].priv,   voters["Bob"].pub);
    bc.addVote("Carol", "CandidateA", ELECTION, voters["Carol"].priv, voters["Carol"].pub);

    // Double-vote attempt — should be blocked
    bc.addVote("Alice", "CandidateB", ELECTION, voters["Alice"].priv, voters["Alice"].pub);

    // Flush any pending txs to chain
    bc.flush();

    // --- Tally ---
    std::cout << "\n=== Results for " << ELECTION << " ===\n";
    std::unordered_map<std::string, int> tally;
    for (const auto& block : bc.chain())
        for (const auto& tx : block.transactions)
            if (tx.electionID == ELECTION)
                tally[tx.candidate]++;

    for (const auto& [candidate, count] : tally)
        std::cout << "  " << candidate << ": " << count << " vote(s)\n";

    // --- Full chain audit ---
    std::cout << "\n[audit] chain valid: " << (bc.isValid() ? "YES" : "NO") << "\n";

    return 0;
}