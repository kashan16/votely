/**
 * bindings.cpp
 *
 * Emscripten bindings that expose the Blockchain class to JavaScript.
 *
 * Memory contract
 * ───────────────
 * Functions that return a char* allocate via strdup().
 * JavaScript must call free_string(ptr) once it has consumed the value.
 */

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "blockchain.hpp"
#include "picosha2.h"

#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>

// ─── Global chain instance ────────────────────────────────
static std::unique_ptr<Blockchain> g_chain;

extern "C" {

// ─── Lifecycle ────────────────────────────────────────────

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void blockchain_init(int difficulty) {
    g_chain = std::make_unique<Blockchain>(difficulty);
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int blockchain_load(const char* json) {
    try {
        g_chain = std::make_unique<Blockchain>(Blockchain::deserialize(json));
        return 1;
    } catch (...) {
        return 0;
    }
}

// ─── Core operations ─────────────────────────────────────

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
char* blockchain_add_vote(const char* election_id,
                          const char* voter_hash,
                          const char* candidate_id) {
    if (!g_chain) return nullptr;
    try {
        VoteData v;
        v.election_id  = election_id;
        v.voter_hash   = voter_hash;
        v.candidate_id = candidate_id;
        std::string h = g_chain->addVote(v);
        return strdup(h.c_str());
    } catch (...) {
        return nullptr;
    }
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int blockchain_is_valid() {
    if (!g_chain) return 0;
    return g_chain->isValid() ? 1 : 0;
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
char* blockchain_serialize() {
    if (!g_chain) return nullptr;
    return strdup(g_chain->serialize().c_str());
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
char* blockchain_tally(const char* election_id) {
    if (!g_chain) return nullptr;
    return strdup(g_chain->tallyVotes(election_id).c_str());
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
int blockchain_size() {
    if (!g_chain) return 0;
    return g_chain->size();
}

// ─── Merkle tree ─────────────────────────────────────────

/**
 * Compute and return the Merkle root of the current chain.
 *
 * The root is SHA-256(left||right) reduced level-by-level from all
 * block hashes, identical to Bitcoin's transaction Merkle tree.
 * A single tampered block changes every ancestor hash up to the root,
 * so this 64-char hex string is a compact integrity fingerprint.
 *
 * Caller must pass the pointer to free_string() when done.
 */
#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
char* blockchain_merkle_root() {
    if (!g_chain) return nullptr;
    return strdup(g_chain->merkleRoot().c_str());
}

// ─── Utility ─────────────────────────────────────────────

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
char* sha256(const char* input) {
    return strdup(picosha2::hash256_hex_string(std::string(input)).c_str());
}

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
#endif
void free_string(char* ptr) {
    free(ptr);
}

} // extern "C"