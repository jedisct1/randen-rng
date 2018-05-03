
#include "randen.h"

#include <stdint.h>
#include <string.h>

#define FEISTEL_BLOCKS 16
#define FEISTEL_FUNCTIONS (FEISTEL_BLOCKS / 2)
#define FEISTEL_ROUNDS (16 + 1)
#define KEYS (FEISTEL_ROUNDS * FEISTEL_FUNCTIONS)
#define CAPACITY_BYTES 16
#define STATE_BYTES 256
#define RATE_BYTES (STATE_BYTES - CAPACITY_BYTES)

#if !(defined(__clang__) || defined(__GNUC__)) && !defined(__restrict__)
# define __restrict__
#endif

#include "private/randen_vector128.h"
#include "private/randen_round_keys.h"

static inline void
block_shuffle(uint64_t *__restrict__ state)
{
    static const int shuffle[FEISTEL_ROUNDS] = { 7,  2, 13, 4,  11, 8,  3, 6,
                                                 15, 0, 9,  10, 1,  14, 5, 12 };
    uint64_t         source[FEISTEL_BLOCKS * LANES];
    int              branch;

    memcpy(source, state, sizeof source);
    for (branch = 0; branch < FEISTEL_BLOCKS; branch++) {
        const int128 v = load(source, shuffle[branch]);
        store(v, state, branch);
    }
}

static inline void
permute(uint64_t *__restrict__ state)
{
    const uint64_t *__restrict__ keys = ROUND_KEYS;
    int round;
    int branch;

#ifdef __clang__
#pragma clang loop unroll_count(2)
#endif
    for (round = 0; round < FEISTEL_ROUNDS; round++) {
        for (branch = 0; branch < FEISTEL_BLOCKS; branch += 2) {
            const int128 even = load(state, branch);
            const int128 odd  = load(state, branch + 1);
            const int128 f1   = aes(even, load(keys, 0));
            const int128 f2   = aes(f1, odd);
            keys += LANES;
            store(f2, state, branch + 1);
        }
        block_shuffle(state);
    }
}

static void
absorb(const void *seed_void, void *state_void)
{
    const uint64_t *__restrict__ seed = (uint64_t *) seed_void;
    uint64_t *__restrict__ state      = (uint64_t *) state_void;
    const int capacity_blocks         = CAPACITY_BYTES / sizeof(int128);
    int       i;

    for (i = capacity_blocks; i < STATE_BYTES / (int) sizeof(int128); i++) {
        int128 block = load(state, i);
        block ^= load(seed, i - capacity_blocks);
        store(block, state, i);
    }
}

static void
generate(void *state_void)
{
    uint64_t *__restrict__ state = (uint64_t *) state_void;
    int128 prev_inner            = load(state, 0);
    int128 inner;

    permute(state);
    inner = load(state, 0);
    inner ^= prev_inner;
    store(inner, state, 0);
}

void
randen_reseed(RandenState *st, const uint8_t seed[RANDEN_SEED_BYTES])
{
    absorb(seed, st->state);
}

void
randen_init(RandenState *st, const uint8_t seed[RANDEN_SEED_BYTES])
{
    memset(st, 0, sizeof *st);
    st->next = STATE_BYTES;
    randen_reseed(st, seed);
}

uint8_t
randen_generate_byte(RandenState *st)
{
    uint64_t next = st->next;
    uint8_t  ret;

    if (next >= STATE_BYTES) {
        generate(st->state);
        next = CAPACITY_BYTES;
    }
    ret      = st->state[next];
    st->next = next + 1;

    return ret;
}

void
randen_generate(RandenState *st, uint8_t *out, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        out[i] = randen_generate_byte(st);
    }
}
