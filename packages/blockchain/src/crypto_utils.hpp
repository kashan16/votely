#pragma once
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <string>
#include <stdexcept>

inline void handleOpenSSLErrors() {
    ERR_print_errors_fp(stderr);
    abort();
}

// ── Base64 decode ─────────────────────────────────────────────────────────────
// The browser (Web Crypto API) produces a raw binary signature which the client
// encodes as Base64 for JSON transport. The C++ binary receives the Base64
// string and must decode it back to raw bytes before calling EVP_DigestVerifyFinal.
inline std::string base64Decode(const std::string& encoded) {
    BIO* b64  = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);   // input has no newlines
    BIO* bmem = BIO_new_mem_buf(encoded.data(), (int)encoded.size());
    bmem      = BIO_push(b64, bmem);

    // Decoded output is always <= input length
    std::string decoded(encoded.size(), '\0');
    int len = BIO_read(bmem, &decoded[0], (int)decoded.size());
    BIO_free_all(bmem);

    if (len < 0) throw std::runtime_error("base64Decode: BIO_read failed");
    decoded.resize((size_t)len);
    return decoded;
}

inline void generateKeyPair(std::string& publicKey, std::string& privateKey) {
    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, NULL);
    if (!ctx) handleOpenSSLErrors();
    if (EVP_PKEY_keygen_init(ctx) <= 0) handleOpenSSLErrors();
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048) <= 0) handleOpenSSLErrors();

    EVP_PKEY* pkey = NULL;
    if (EVP_PKEY_keygen(ctx, &pkey) <= 0) handleOpenSSLErrors();
    EVP_PKEY_CTX_free(ctx);

    auto readBIO = [](BIO* bio) -> std::string {
        int len = BIO_pending(bio);
        std::string out(len, '\0');
        BIO_read(bio, &out[0], len);
        return out;
    };

    BIO* bp_priv = BIO_new(BIO_s_mem());
    if (!PEM_write_bio_PrivateKey(bp_priv, pkey, NULL, NULL, 0, NULL, NULL))
        handleOpenSSLErrors();
    privateKey = readBIO(bp_priv);
    BIO_free_all(bp_priv);

    BIO* bp_pub = BIO_new(BIO_s_mem());
    if (!PEM_write_bio_PUBKEY(bp_pub, pkey)) handleOpenSSLErrors();
    publicKey = readBIO(bp_pub);
    BIO_free_all(bp_pub);

    EVP_PKEY_free(pkey);
}

inline std::string signMessage(const std::string& message, const std::string& privateKeyStr) {
    BIO* bio = BIO_new_mem_buf(privateKeyStr.c_str(), -1);
    EVP_PKEY* privateKey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (!privateKey) handleOpenSSLErrors();

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(mdctx, NULL, EVP_sha256(), NULL, privateKey);
    EVP_DigestSignUpdate(mdctx, message.c_str(), message.size());

    size_t sigLen;
    EVP_DigestSignFinal(mdctx, NULL, &sigLen);
    std::string sig(sigLen, '\0');
    EVP_DigestSignFinal(mdctx, (unsigned char*)&sig[0], &sigLen);
    sig.resize(sigLen);

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(privateKey);
    return sig;
}

// publicKeyStr MUST be a PEM string, i.e.:
//   -----BEGIN PUBLIC KEY-----
//   <base64 lines>
//   -----END PUBLIC KEY-----
// The browser exports keys as raw SPKI Base64 — wrap them in PEM headers
// before storing in Supabase (see UseAuth.ts: generateAndStoreKeyPair).
//
// signature must be raw bytes (NOT Base64).
// Call base64Decode() on the incoming signature first (see voting_system.cpp).
inline bool verifySignature(const std::string& message, const std::string& signature,
                             const std::string& publicKeyStr) {
    BIO* bio = BIO_new_mem_buf(publicKeyStr.c_str(), -1);
    EVP_PKEY* publicKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (!publicKey) {
        std::cerr << "[crypto] PEM_read_bio_PUBKEY failed — is the key in PEM format?\n";
        ERR_print_errors_fp(stderr);
        return false;
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, publicKey);
    EVP_DigestVerifyUpdate(mdctx, message.c_str(), message.size());
    bool ok = EVP_DigestVerifyFinal(
        mdctx, (const unsigned char*)signature.data(), signature.size()) == 1;

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(publicKey);
    return ok;
}