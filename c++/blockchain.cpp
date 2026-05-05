#include "blockchain.hpp"
#include "picosha2.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

// ═══════════════════════════════════════════════════════════
//  Tiny JSON helpers
//  We avoid a full JSON library dependency to keep things
//  self-contained.  These are intentionally minimal.
// ═══════════════════════════════════════════════════════════

static std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

static std::string jsonGetStr(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    std::string val;
    bool escaped = false;
    for (size_t i = pos; i < json.size(); ++i) {
        char c = json[i];
        if (escaped) { val += c; escaped = false; continue; }
        if (c == '\\') { escaped = true; continue; }
        if (c == '"') break;
        val += c;
    }
    return val;
}

static long long jsonGetInt(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos)
        throw std::runtime_error("Key not found: " + key);
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') ++pos;
    std::string numStr;
    if (pos < json.size() && json[pos] == '-') { numStr += '-'; ++pos; }
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos])))
        numStr += json[pos++];
    if (numStr.empty() || numStr == "-")
        throw std::runtime_error("Bad integer for key: " + key);
    return std::stoll(numStr);
}

// ═══════════════════════════════════════════════════════════
//  Block implementation
// ═══════════════════════════════════════════════════════════

std::string Block::votePayload() const {
    return vote.election_id + "|" + vote.voter_hash + "|" + vote.candidate_id;
}

std::string Block::toHashInput() const {
    return std::to_string(index)
         + std::to_string(static_cast<long long>(timestamp))
         + votePayload()
         + prev_hash
         + std::to_string(nonce);
}

std::string Block::computeHash() const {
    return picosha2::hash256_hex_string(toHashInput());
}

void Block::mine(int difficulty) {
    std::string target(difficulty, '0');
    nonce = 0;
    do {
        hash = computeHash();
        ++nonce;
    } while (hash.substr(0, difficulty) != target);
    --nonce;
}

std::string Block::toJson() const {
    std::ostringstream o;
    o << "{"
      << "\"index\":"      << index                          << ","
      << "\"timestamp\":"  << static_cast<long long>(timestamp) << ","
      << "\"nonce\":"      << nonce                          << ","
      << "\"prev_hash\":\"" << jsonEscape(prev_hash)         << "\","
      << "\"hash\":\""      << jsonEscape(hash)              << "\","
      << "\"election_id\":\"" << jsonEscape(vote.election_id)   << "\","
      << "\"voter_hash\":\"" << jsonEscape(vote.voter_hash)     << "\","
      << "\"candidate_id\":\"" << jsonEscape(vote.candidate_id) << "\""
      << "}";
    return o.str();
}

Block Block::fromJson(const std::string& json) {
    Block b;
    b.index       = static_cast<int>(jsonGetInt(json, "index"));
    b.timestamp   = static_cast<std::time_t>(jsonGetInt(json, "timestamp"));
    b.nonce       = static_cast<int>(jsonGetInt(json, "nonce"));
    b.prev_hash   = jsonGetStr(json, "prev_hash");
    b.hash        = jsonGetStr(json, "hash");
    b.vote.election_id  = jsonGetStr(json, "election_id");
    b.vote.voter_hash   = jsonGetStr(json, "voter_hash");
    b.vote.candidate_id = jsonGetStr(json, "candidate_id");
    return b;
}

// ═══════════════════════════════════════════════════════════
//  Blockchain implementation
// ═══════════════════════════════════════════════════════════

Blockchain::Blockchain(int difficulty)
    : difficulty_(difficulty)
{
    chain_.push_back(createGenesis());
}

Block Blockchain::createGenesis() const {
    Block g;
    g.index     = 0;
    g.timestamp = 0;
    g.nonce     = 0;
    g.prev_hash = std::string(64, '0');
    g.vote      = {"GENESIS", "GENESIS", "GENESIS"};
    g.hash      = g.computeHash();
    return g;
}

std::string Blockchain::addVote(const VoteData& vote) {
    Block b;
    b.index     = static_cast<int>(chain_.size());
    b.timestamp = std::time(nullptr);
    b.vote      = vote;
    b.prev_hash = lastBlock().hash;
    b.nonce     = 0;
    b.mine(difficulty_);
    chain_.push_back(b);
    return b.hash;
}

bool Blockchain::isValid() const {
    std::string target(difficulty_, '0');

    for (size_t i = 1; i < chain_.size(); ++i) {
        const Block& cur  = chain_[i];
        const Block& prev = chain_[i - 1];

        if (cur.computeHash() != cur.hash)   return false;
        if (cur.prev_hash != prev.hash)       return false;
        if (cur.hash.substr(0, difficulty_) != target) return false;
    }
    return true;
}

std::string Blockchain::tallyVotes(const std::string& election_id) const {
    std::map<std::string, int> counts;

    for (size_t i = 1; i < chain_.size(); ++i) {
        const Block& b = chain_[i];
        if (b.vote.election_id == election_id)
            counts[b.vote.candidate_id]++;
    }

    std::ostringstream o;
    o << "{";
    bool first = true;
    for (const auto& kv : counts) {
        if (!first) o << ",";
        o << "\"" << jsonEscape(kv.first) << "\":" << kv.second;
        first = false;
    }
    o << "}";
    return o.str();
}

// ═══════════════════════════════════════════════════════════
//  Merkle tree
//
//  Algorithm (identical to Bitcoin's block-transaction tree):
//    Level 0 : leaf hashes = every block's stored hash
//    Each subsequent level : SHA-256(left || right)
//              If the level has an odd node, duplicate it.
//    Root    : the single hash that remains after all levels.
//
//  This gives a single 64-char hex "fingerprint" of the entire
//  chain that changes if *any* block is tampered with.
// ═══════════════════════════════════════════════════════════

std::string Blockchain::merkleRoot() const {
    if (chain_.empty()) return std::string(64, '0');

    // Seed with all block hashes (genesis included)
    std::vector<std::string> level;
    level.reserve(chain_.size());
    for (const auto& b : chain_) {
        level.push_back(b.hash);
    }

    // Reduce level-by-level until one hash remains
    while (level.size() > 1) {
        // Pad to even length by duplicating the last element
        if (level.size() % 2 != 0)
            level.push_back(level.back());

        std::vector<std::string> next;
        next.reserve(level.size() / 2);
        for (size_t i = 0; i < level.size(); i += 2) {
            next.push_back(
                picosha2::hash256_hex_string(level[i] + level[i + 1])
            );
        }
        level = std::move(next);
    }

    return level[0];
}

std::string Blockchain::serialize() const {
    std::ostringstream o;
    o << "[";
    for (size_t i = 0; i < chain_.size(); ++i) {
        if (i > 0) o << ",";
        o << chain_[i].toJson();
    }
    o << "]";
    return o.str();
}

Blockchain Blockchain::deserialize(const std::string& json, int difficulty) {
    Blockchain bc(difficulty);
    bc.chain_.clear();

    int depth = 0;
    std::string current;
    bool inString = false;
    bool escaped  = false;

    for (char c : json) {
        if (escaped) { current += c; escaped = false; continue; }
        if (c == '\\' && inString) { current += c; escaped = true; continue; }
        if (c == '"') inString = !inString;

        if (!inString) {
            if (c == '{') { depth++; current += c; continue; }
            if (c == '}') {
                current += c;
                if (--depth == 0) {
                    bc.chain_.push_back(Block::fromJson(current));
                    current.clear();
                }
                continue;
            }
        }
        if (depth > 0) current += c;
    }

    if (bc.chain_.empty())
        throw std::runtime_error("Deserialized chain is empty");

    return bc;
}

const Block& Blockchain::getBlock(int index) const {
    if (index < 0 || index >= static_cast<int>(chain_.size()))
        throw std::out_of_range("Block index out of range");
    return chain_[index];
}