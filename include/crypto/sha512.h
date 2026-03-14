#ifndef CRYPTO_SHA512_H
#define CRYPTO_SHA512_H

#include <types.h>

#define SHA512_BLOCK_SIZE 128

typedef struct {
    uint64_t state[8];
    uint64_t bitlen;
    uint8_t data[128];
    uint32_t datalen;
} sha512_ctx;

void sha512_init(sha512_ctx *ctx);
void sha512_update(sha512_ctx *ctx, const uint8_t *data, size_t len);
void sha512_final(sha512_ctx *ctx, uint8_t hash[64]);

/* Helper function to hash a buffer in one go */
void sha512(const uint8_t *data, size_t len, uint8_t hash[64]);

#endif
