#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

// Crypto Utils

void handleOpenSSLErrors() {
    ERR_print_errors_fp(stderr);
    abort();
}

void generateKeyPair(std::string& publicKey, std::string& privateKey) {
    EVP_PKEY_CTX *ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) handleOpenSSLErrors();
    if (EVP_PKEY_keygen_init(ctx) <= 0) handleOpenSSLErrors();
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) handleOpenSSLErrors();
    EVP_PKEY *pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) handleOpenSSLErrors();
    EVP_PKEY_CTX_free(ctx);

    BIO *bp_private = BIO_new(BIO_s_mem());
    if (!PEM_write_bio_PrivateKey(bp_private, pkey, NULL, NULL, 0, NULL, NULL)) handleOpenSSLErrors();
    int privateKeyLen = BIO_pending(bp_private);
    char *privateKeyCStr = new char[privateKeyLen + 1];
    BIO_read(bp_private, privateKeyCStr, privateKeyLen);
    privateKeyCStr[privateKeyLen] = '\0';
    privateKey = std::string(privateKeyCStr);

    BIO *bp_public = BIO_new(BIO_s_mem());
    if (!PEM_write_bio_PUBKEY(bp_public, pkey)) handleOpenSSLErrors();
    int publicKeyLen = BIO_pending(bp_public);
    char *publicKeyCStr = new char[publicKeyLen + 1];
    BIO_read(bp_public, publicKeyCStr, publicKeyLen);
    publicKeyCStr[publicKeyLen] = '\0';
    publicKey = std::string(publicKeyCStr);

    EVP_PKEY_free(pkey);
    BIO_free_all(bp_private);
    BIO_free_all(bp_public);
    delete[] privateKeyCStr;
    delete[] publicKeyCStr;
}

std::string signMessage(const std::string& message, const std::string& privateKeyStr) {
    BIO *bio = BIO_new_mem_buf(privateKeyStr.c_str(), -1);
    EVP_PKEY *privateKey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    if (!privateKey) handleOpenSSLErrors();

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) handleOpenSSLErrors();
    if (EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, privateKey) <= 0) handleOpenSSLErrors();
    if (EVP_DigestSignUpdate(mdctx, message.c_str(), message.size()) <= 0) handleOpenSSLErrors();

    size_t sigLen;
    if (EVP_DigestSignFinal(mdctx, NULL, &sigLen) <= 0) handleOpenSSLErrors();
    unsigned char* sig = new unsigned char[sigLen];
    if (EVP_DigestSignFinal(mdctx, sig, &sigLen) <= 0) handleOpenSSLErrors();

    std::string signature(reinterpret_cast<char*>(sig), sigLen);
    delete[] sig;
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(privateKey);
    BIO_free(bio);
    return signature;
}

bool verifyMessage(const std::string& message, const std::string& signature, const std::string& publicKeyStr) {
    BIO *bio = BIO_new_mem_buf(publicKeyStr.c_str(), -1);
    EVP_PKEY *publicKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    if (!publicKey) handleOpenSSLErrors();

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) handleOpenSSLErrors();
    if (EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, publicKey) <= 0) handleOpenSSLErrors();
    if (EVP_DigestVerifyUpdate(mdctx, message.c_str(), message.size()) <= 0) handleOpenSSLErrors();

    bool result = EVP_DigestVerifyFinal(mdctx, (unsigned char*)signature.c_str(), signature.size()) == 1;
    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(publicKey);
    BIO_free(bio);
    return result;
}

// Blockchain

struct Transaction {
    std::string voterID;
    std::string candidate;
    std::string signature;
};

struct Block {
    int index;
    std::vector<Transaction> transactions;
    std::string previousHash;
    std::string hash;
    time_t timestamp;
    int nonce;
};

std::string calculateHash(const Block& block) {
    std::stringstream ss;
    ss << block.index << block.previousHash << block.timestamp << block.nonce;
    for (const auto& tx : block.transactions) {
        ss << tx.voterID << tx.candidate << tx.signature;
    }
    std::string data = ss.str();
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)&data[0], data.size(), hash);
    std::stringstream hashString;
    for (int i = 0; i < SHA256_DIGEST_LENGTH; ++i) {
        hashString << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return hashString.str();
}

void mineBlock(Block& block, int difficulty) {
    std::string target(difficulty, '0');
    while (block.hash.substr(0, difficulty) != target) {
        block.nonce++;
        block.hash = calculateHash(block);
    }
    std::cout << "Block mined: " << block.hash << std::endl;
}

class Blockchain {
public:
    Blockchain(int difficulty) : difficulty(difficulty) {
        chain.push_back(createGenesisBlock());
    }

    void addBlock(Block newBlock) {
        newBlock.previousHash = getLatestBlock().hash;
        mineBlock(newBlock, difficulty);
        chain.push_back(newBlock);
    }

    const std::vector<Block>& getChain() const {
        return chain;
    }
    
    Block getLatestBlock() const {
        return chain.back();
    }

private:
    std::vector<Block> chain;
    int difficulty;

    Block createGenesisBlock() {
        Block genesis;
        genesis.index = 0;
        genesis.previousHash = "0";
        genesis.timestamp = std::time(nullptr);
        genesis.nonce = 0;
        genesis.hash = calculateHash(genesis);
        return genesis;
    }
};

class VotingSystem {
public:
    VotingSystem(int difficulty) : blockchain(difficulty) {}

    void registerVoter(const std::string& voterID) {
        voters.push_back(voterID);
    }

    void castVote(const std::string& voterID, const std::string& candidate, const std::string& privateKey) {
        if (isRegistered(voterID)) {
            std::string message = voterID + candidate;
            std::string signature = signMessage(message, privateKey);
            Transaction tx = {voterID, candidate, signature};
            pendingTransactions.push_back(tx);
            if (pendingTransactions.size() >= blockSize) {
                Block newBlock;
                newBlock.index = blockchain.getLatestBlock().index + 1;
                newBlock.previousHash = blockchain.getLatestBlock().hash;
                newBlock.timestamp = std::time(nullptr);
                newBlock.nonce = 0;
                newBlock.transactions = pendingTransactions;
                blockchain.addBlock(newBlock);
                pendingTransactions.clear();
            }
            std::cout << "Vote cast by " << voterID << " for " << candidate << std::endl;
        } else {
            std::cout << "Voter not registered" << std::endl;
        }
    }

    void verifyVotes() const {
        for (const auto& block : blockchain.getChain()) {
            std::cout << "Block #" << block.index << ", Hash: " << block.hash << std::endl;
            for (const auto& tx : block.transactions) {
                std::cout << "  Voter: " << tx.voterID << ", Voted for: " << tx.candidate << std::endl;
            }
            std::cout << std::endl;
        }
    }

private:
    std::vector<std::string> voters;
    std::vector<Transaction> pendingTransactions;
    Blockchain blockchain;
    int blockSize = 2; // Adjust as needed

    bool isRegistered(const std::string& voterID) const {
        return std::find(voters.begin(), voters.end(), voterID) != voters.end();
    }
};

int main() {
    // Example usage
    VotingSystem vs(4); // Difficulty of 4 for mining
    std::string voter1PublicKey, voter1PrivateKey;
    generateKeyPair(voter1PublicKey, voter1PrivateKey);
    std::string voter2PublicKey, voter2PrivateKey;
    generateKeyPair(voter2PublicKey, voter2PrivateKey);

    vs.registerVoter("Voter1");
    vs.registerVoter("Voter2");

    vs.castVote("Voter1", "CandidateA", voter1PrivateKey);
    vs.castVote("Voter2", "CandidateB", voter2PrivateKey);

    vs.verifyVotes();

    return 0;
}