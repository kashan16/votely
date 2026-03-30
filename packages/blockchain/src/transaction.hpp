#pragma once
#include <string>

struct Transaction {
    std::string voterID;
    std::string candidate;
    std::string electionID;
    std::string signature;
    long long timestamp;

    bool operator==(const Transaction& o) const {
        return voterID == o.voterID && electionID == o.electionID;
    }
};