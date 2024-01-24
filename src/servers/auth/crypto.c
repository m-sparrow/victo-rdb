#include "interface/crypto.h"
#include "../../utils/logs/interface/log.h"

char* sha256(const char *input) {
    logWriter(LOG_DEBUG, "crypto sha256 started");

    EVP_MD_CTX *mdctx;
    unsigned char *hash = malloc(SHA256_DIGEST_LENGTH);

    if (hash == NULL) {
        logWriter(LOG_DEBUG, "Memory allocation failed while allocating memory for sha256 hash");
        return NULL;
    }

    if ((mdctx = EVP_MD_CTX_new()) == NULL) {
        logWriter(LOG_DEBUG, "Error creating context for sha256");
        return NULL;
    }

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), NULL) != 1) {
        logWriter(LOG_DEBUG, "Error initializing digest for sha256");
        return NULL;
    }

    if (EVP_DigestUpdate(mdctx, input, strlen(input)) != 1) {
        logWriter(LOG_DEBUG, "Error updating digest for sha256");
        return NULL;
    }

    if (EVP_DigestFinal_ex(mdctx, hash, NULL) != 1) {
        logWriter(LOG_DEBUG, "Error finalizing digest for sha256");
        return NULL;
    }

    char *hexHash = malloc(2 * SHA256_DIGEST_LENGTH + 1); 
    if (hexHash == NULL) {
        free(hash);
        EVP_MD_CTX_free(mdctx);
        logWriter(LOG_DEBUG, "Memory allocation failed while allocating memory for sha256 hex hash");
        return NULL;
    }

    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(hexHash + (i * 2), "%02x", hash[i]);
    }

    hexHash[2 * SHA256_DIGEST_LENGTH] = '\0';

    free(hash);
    EVP_MD_CTX_free(mdctx);
    logWriter(LOG_DEBUG, "crypto sha256 completed");

    return hexHash;
}