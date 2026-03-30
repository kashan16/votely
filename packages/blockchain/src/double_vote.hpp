#pragma once
#include <string>
#include <unordered_set>

// Tracks (voterID + electionID) pairs already committed to the chain.
// This is the C++-layer guard — Supabase unique constraint is the second layer.
class DoubleVoteGuard {
public:
    bool hasVoted(const std::string& voterID, const std::string& electionID) const {
        return seen_.count(key(voterID, electionID)) > 0;
    }

    void markVoted(const std::string& voterID, const std::string& electionID) {
        seen_.insert(key(voterID, electionID));
    }

    void rebuild(const std::vector<Transaction>& txs) {
        seen_.clear();
        for (const auto& tx : txs)
            markVoted(tx.voterID, tx.electionID);
    }

private:
    std::unordered_set<std::string> seen_;
    static std::string key(const std::string& v, const std::string& e) {
        return v + "::" + e;
    }
};