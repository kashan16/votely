#pragma once
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/sha.h>
#include <string>
#include <stdexcept>

inline void handleOpenSSLErrors() {
    ERR_print_errors_fp(stderr);
    abort();
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

inline bool verifySignature(const std::string& message, const std::string& signature,
                             const std::string& publicKeyStr) {
    BIO* bio = BIO_new_mem_buf(publicKeyStr.c_str(), -1);
    EVP_PKEY* publicKey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
    BIO_free(bio);
    if (!publicKey) return false;

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    EVP_DigestVerifyInit(mdctx, NULL, EVP_sha256(), NULL, publicKey);
    EVP_DigestVerifyUpdate(mdctx, message.c_str(), message.size());
    bool ok = EVP_DigestVerifyFinal(
        mdctx, (unsigned char*)signature.c_str(), signature.size()) == 1;

    EVP_MD_CTX_free(mdctx);
    EVP_PKEY_free(publicKey);
    return ok;
}