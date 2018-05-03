#ifndef randen_H
#define randen_H

#include <stddef.h>
#include <stdint.h>

#define RANDEN_STATE_BYTES 256
#define RANDEN_SEED_BYTES (RANDEN_STATE_BYTES - 16)

#ifndef CRYPTO_ALIGN
# if defined(__INTEL_COMPILER) || defined(_MSC_VER)
#  define CRYPTO_ALIGN(x) __declspec(align(x))
# else
#  define CRYPTO_ALIGN(x) __attribute__((aligned(x)))
# endif
#endif

typedef struct RandenState_ {
    CRYPTO_ALIGN(32) uint8_t state[RANDEN_STATE_BYTES];
    uint64_t next;
} RandenState;

void    randen_init(RandenState *st, const uint8_t seed[RANDEN_SEED_BYTES]);
void    randen_reseed(RandenState *st, const uint8_t seed[RANDEN_SEED_BYTES]);
uint8_t randen_generate_byte(RandenState *st);
void    randen_generate(RandenState *st, uint8_t *out, size_t len);

#endif
