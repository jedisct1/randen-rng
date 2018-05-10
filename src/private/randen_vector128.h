#ifndef randen_vector128_H
#define randen_vector128_H

#if defined(__SSE2__) && defined(__AES__)

# include <wmmintrin.h>

typedef __m128i int128;

# define LANES (16 / 8)

static inline int128
load(const uint64_t *__restrict__ lanes, const int block)
{
    const uint64_t *__restrict__ from = lanes + block * LANES;
    return _mm_load_si128((const int128 *) from);
}

static inline void
store(const int128 v, uint64_t *__restrict__ lanes, const int block)
{
    uint64_t *__restrict__ to = lanes + block * LANES;
    return _mm_store_si128((int128 *) to, v);
}

static inline int128
aes(const int128 state, const int128 round_key)
{
    return _mm_aesenc_si128(state, round_key);
}

# elif defined(__ARM_NEON) && defined(__ARM_FEATURE_CRYPTO)

#include <arm_neon.h>

typedef uint8x16_t int128;

# define LANES (16 / 8)

static inline int128
load(const uint64_t *__restrict__ lanes, const int block)
{
    const uint8_t *__restrict__ from =
     (const uint8_t *) (lanes + block * LANES);
    return vld1q_u8(from);
}

static inline void
store(const int128 v, uint64_t *__restrict__ lanes, const int block)
{
    uint8_t *__restrict__ to = (uint8_t *) (lanes + block * LANES);
    return vst1q_u8(to, v);
}

static inline int128
aes(const int128 state, const int128 round_key)
{
    return vaesmcq_u8(vaeseq_u8(state, round_key));
}

# else

#  error Architecture not supported. Or try compiling with -march=native

# endif

#endif
