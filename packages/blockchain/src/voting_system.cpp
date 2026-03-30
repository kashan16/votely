#include <iostream>
#include <string>
#include <unordered_map>
#include "blockchain.hpp"
#include "crypto_utils.hpp"
#include "json.hpp"

using json = nlohmann::json;

// -------------------------------------------------------------------
// JSON CLI mode — used by the Node.js API bridge
// Usage: ./voting_system <command> '<json_payload>'
//
// Commands:
//   cast       {"voterID","candidate","electionID","signature","publicKey"}
//   results    {"electionID"}
//   chain      (no payload)
//   validate   (no payload)
// -------------------------------------------------------------------
int runCLI(const std::string& cmd, const std::string& payload) {
    try {
        Blockchain bc(3, "chain.json");

        if (cmd == "cast") {
            auto j = json::parse(payload);
            std::string voterID    = j["voterID"];
            std::string candidate  = j["candidate"];
            std::string electionID = j["electionID"];
            std::string signature  = j["signature"];   // raw bytes (base64-decoded by Node)
            std::string publicKey  = j["publicKey"];   // PEM string

            // Verify the signature before touching the chain
            std::string msg = voterID + candidate + electionID;
            if (!verifySignature(msg, signature, publicKey)) {
                std::cout << json{{"ok", false}, {"error", "signature_invalid"}}.dump() << "\n";
                return 1;
            }

            // Guard checks double-vote internally
            if (bc.addVoteVerified(voterID, candidate, electionID, signature)) {
                bc.flush();
                std::string blockHash = bc.chain().back().hash;
                std::cout << json{{"ok", true}, {"blockHash", blockHash}}.dump() << "\n";
            } else {
                std::cout << json{{"ok", false}, {"error", "double_vote"}}.dump() << "\n";
            }
        }
        else if (cmd == "results") {
            auto j = json::parse(payload);
            std::string electionID = j["electionID"];

            std::unordered_map<std::string, int> tally;
            for (const auto& block : bc.chain())
                for (const auto& tx : block.transactions)
                    if (tx.electionID == electionID)
                        tally[tx.candidate]++;

            json out = json::object();
            for (const auto& [k, v] : tally) out[k] = v;
            std::cout << json{{"ok", true}, {"tally", out}}.dump() << "\n";
        }
        else if (cmd == "chain") {
            json blocks = json::array();
            for (const auto& b : bc.chain()) {
                json txs = json::array();
                for (const auto& tx : b.transactions)
                    txs.push_back({{"voterID", tx.voterID},
                                   {"candidate", tx.candidate},
                                   {"electionID", tx.electionID},
                                   {"timestamp", tx.timestamp}});
                blocks.push_back({{"index", b.index},
                                   {"hash", b.hash},
                                   {"previousHash", b.previousHash},
                                   {"merkleRoot", b.merkleRoot},
                                   {"timestamp", (long long)b.timestamp},
                                   {"nonce", b.nonce},
                                   {"transactions", txs}});
            }
            std::cout << json{{"ok", true}, {"chain", blocks}}.dump() << "\n";
        }
        else if (cmd == "validate") {
            bool valid = bc.isValid();
            std::cout << json{{"ok", true}, {"valid", valid}}.dump() << "\n";
        }
        else {
            std::cout << json{{"ok", false}, {"error", "unknown_command"}}.dump() << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << json{{"ok", false}, {"error", e.what()}}.dump() << "\n";
        return 1;
    }
    return 0;
}

// -------------------------------------------------------------------
// Demo mode — kept for manual testing
// -------------------------------------------------------------------
int runDemo() {
    Blockchain bc(3, "chain.json");

    struct Voter { std::string pub, priv; };
    std::unordered_map<std::string, Voter> voters;
    for (const std::string& id : {"Alice", "Bob", "Carol"}) {
        Voter v;
        generateKeyPair(v.pub, v.priv);
        voters[id] = v;
        std::cout << "[reg] voter registered: " << id << "\n";
    }

    const std::string ELECTION = "election-2025-general";
    bc.addVote("Alice", "CandidateA", ELECTION, voters["Alice"].priv, voters["Alice"].pub);
    bc.addVote("Bob",   "CandidateB", ELECTION, voters["Bob"].priv,   voters["Bob"].pub);
    bc.addVote("Carol", "CandidateA", ELECTION, voters["Carol"].priv, voters["Carol"].pub);
    bc.addVote("Alice", "CandidateB", ELECTION, voters["Alice"].priv, voters["Alice"].pub); // blocked
    bc.flush();

    std::unordered_map<std::string, int> tally;
    for (const auto& block : bc.chain())
        for (const auto& tx : block.transactions)
            if (tx.electionID == ELECTION)
                tally[tx.candidate]++;

    std::cout << "\n=== Results ===\n";
    for (const auto& [c, n] : tally)
        std::cout << "  " << c << ": " << n << "\n";
    std::cout << "[audit] valid: " << (bc.isValid() ? "YES" : "NO") << "\n";
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc >= 2) {
        std::string cmd = argv[1];
        std::string payload = argc >= 3 ? argv[2] : "{}";
        return runCLI(cmd, payload);
    }
    return runDemo();
}