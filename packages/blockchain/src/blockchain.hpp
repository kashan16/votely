#pragma once
#include <vector>
#include <stdexcept>
#include "block.hpp"
#include "consensus.hpp"
#include "double_vote.hpp"
#include "validator.hpp"
#include "persistence.hpp"
#include "crypto_utils.hpp"

class Blockchain {
public:

        // Called from CLI mode — signature already externally verified
    bool addVoteVerified(const std::string& voterID, const std::string& candidate,
                            const std::string& electionID, const std::string& signature) {
            if (guard_.hasVoted(voterID, electionID)) return false;
            Transaction tx{voterID, candidate, electionID, signature,
                        (long long)std::time(nullptr)};
            pending_.push_back(tx);
            guard_.markVoted(voterID, electionID);
            if ((int)pending_.size() >= blockSize_) flushBlock();
            return true;
    }
    explicit Blockchain(int difficulty, const std::string& chainFile = "chain.json")
        : difficulty_(difficulty), chainFile_(chainFile) {

        chain_ = loadChain(chainFile_);
        if (chain_.empty()) {
            chain_.push_back(makeGenesis());
            saveChain(chain_, chainFile_);
        } else {
            if (!validateChain(chain_))
                throw std::runtime_error("Chain integrity check failed on load!");
        }
        guard_.rebuild(allTransactions());
    }

    // Returns false if voter already voted in this election
    bool addVote(const std::string& voterID, const std::string& candidate,
                 const std::string& electionID, const std::string& privateKey,
                 const std::string& publicKey) {

        if (guard_.hasVoted(voterID, electionID)) {
            std::cerr << "[chain] double-vote blocked: " << voterID << "\n";
            return false;
        }

        std::string msg = voterID + candidate + electionID;
        std::string sig = signMessage(msg, privateKey);

        if (!verifySignature(msg, sig, publicKey)) {
            std::cerr << "[chain] signature verification failed\n";
            return false;
        }

        Transaction tx{voterID, candidate, electionID, sig,
                       (long long)std::time(nullptr)};
        pending_.push_back(tx);
        guard_.markVoted(voterID, electionID);

        if ((int)pending_.size() >= blockSize_)
            flushBlock();

        return true;
    }

    // Force-commit any pending transactions (call before shutdown)
    void flush() { if (!pending_.empty()) flushBlock(); }

    const std::vector<Block>& chain() const { return chain_; }

    bool isValid() const { return validateChain(chain_); }

private:
    std::vector<Block>  chain_;
    std::vector<Transaction> pending_;
    DoubleVoteGuard     guard_;
    int                 difficulty_;
    int                 blockSize_ = 3;
    std::string         chainFile_;

    Block makeGenesis() {
        Block g;
        g.index        = 0;
        g.previousHash = std::string(64, '0');
        g.timestamp    = std::time(nullptr);
        g.nonce        = 0;
        g.finalise();
        return g;
    }

    void flushBlock() {
        Block b;
        b.index        = (int)chain_.size();
        b.previousHash = chain_.back().hash;
        b.timestamp    = std::time(nullptr);
        b.nonce        = 0;
        b.transactions = pending_;
        mineBlock(b, difficulty_);
        chain_.push_back(b);
        pending_.clear();
        saveChain(chain_, chainFile_);
    }

    std::vector<Transaction> allTransactions() const {
        std::vector<Transaction> all;
        for (const auto& b : chain_)
            for (const auto& tx : b.transactions)
                all.push_back(tx);
        return all;
    }
};