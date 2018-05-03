
#include "randen.h"

#include <stdint.h>
#include <string.h>
#include <wmmintrin.h>

#define INT128 __m128i
#define FEISTEL_BLOCKS 16
#define FEISTEL_FUNCTIONS (FEISTEL_BLOCKS / 2)
#define FEISTEL_ROUNDS (16 + 1)
#define KEYS (FEISTEL_ROUNDS * FEISTEL_FUNCTIONS)
#define LANES (16 / 8)
#define CAPACITY_BYTES 16
#define STATE_BYTES 256
#define RATE_BYTES (STATE_BYTES - CAPACITY_BYTES)

#if !(defined(__clang__) || defined(__GNUC__)) && !defined(__restrict__)
# define __restrict__
#endif

static inline INT128
load(const uint64_t *__restrict__ lanes, const int block)
{
    const uint64_t *__restrict__ from = lanes + block * LANES;
    return _mm_load_si128((const INT128 *) from);
}

static inline void
store(const INT128 v, uint64_t *__restrict__ lanes, const int block)
{
    uint64_t *__restrict__ to = lanes + block * LANES;
    return _mm_store_si128((INT128 *) to, v);
}

static inline INT128
aes(const INT128 state, const INT128 round_key)
{
    return _mm_aesenc_si128(state, round_key);
}

static const uint64_t CRYPTO_ALIGN(32) ROUND_KEYS[KEYS * LANES] = {
    0x243F6A8885A308D3ULL, 0x13198A2E03707344ULL, 0xA4093822299F31D0ULL,
    0x082EFA98EC4E6C89ULL, 0x452821E638D01377ULL, 0xBE5466CF34E90C6CULL,
    0xC0AC29B7C97C50DDULL, 0x3F84D5B5B5470917ULL, 0x9216D5D98979FB1BULL,
    0xD1310BA698DFB5ACULL, 0x2FFD72DBD01ADFB7ULL, 0xB8E1AFED6A267E96ULL,
    0xBA7C9045F12C7F99ULL, 0x24A19947B3916CF7ULL, 0x0801F2E2858EFC16ULL,
    0x636920D871574E69ULL, 0xA458FEA3F4933D7EULL, 0x0D95748F728EB658ULL,
    0x718BCD5882154AEEULL, 0x7B54A41DC25A59B5ULL, 0x9C30D5392AF26013ULL,
    0xC5D1B023286085F0ULL, 0xCA417918B8DB38EFULL, 0x8E79DCB0603A180EULL,
    0x6C9E0E8BB01E8A3EULL, 0xD71577C1BD314B27ULL, 0x78AF2FDA55605C60ULL,
    0xE65525F3AA55AB94ULL, 0x5748986263E81440ULL, 0x55CA396A2AAB10B6ULL,
    0xB4CC5C341141E8CEULL, 0xA15486AF7C72E993ULL, 0xB3EE1411636FBC2AULL,
    0x2BA9C55D741831F6ULL, 0xCE5C3E169B87931EULL, 0xAFD6BA336C24CF5CULL,
    0x7A32538128958677ULL, 0x3B8F48986B4BB9AFULL, 0xC4BFE81B66282193ULL,
    0x61D809CCFB21A991ULL, 0x487CAC605DEC8032ULL, 0xEF845D5DE98575B1ULL,
    0xDC262302EB651B88ULL, 0x23893E81D396ACC5ULL, 0x0F6D6FF383F44239ULL,
    0x2E0B4482A4842004ULL, 0x69C8F04A9E1F9B5EULL, 0x21C66842F6E96C9AULL,
    0x670C9C61ABD388F0ULL, 0x6A51A0D2D8542F68ULL, 0x960FA728AB5133A3ULL,
    0x6EEF0B6C137A3BE4ULL, 0xBA3BF0507EFB2A98ULL, 0xA1F1651D39AF0176ULL,
    0x66CA593E82430E88ULL, 0x8CEE8619456F9FB4ULL, 0x7D84A5C33B8B5EBEULL,
    0xE06F75D885C12073ULL, 0x401A449F56C16AA6ULL, 0x4ED3AA62363F7706ULL,
    0x1BFEDF72429B023DULL, 0x37D0D724D00A1248ULL, 0xDB0FEAD349F1C09BULL,
    0x075372C980991B7BULL, 0x25D479D8F6E8DEF7ULL, 0xE3FE501AB6794C3BULL,
    0x976CE0BD04C006BAULL, 0xC1A94FB6409F60C4ULL, 0x5E5C9EC2196A2463ULL,
    0x68FB6FAF3E6C53B5ULL, 0x1339B2EB3B52EC6FULL, 0x6DFC511F9B30952CULL,
    0xCC814544AF5EBD09ULL, 0xBEE3D004DE334AFDULL, 0x660F2807192E4BB3ULL,
    0xC0CBA85745C8740FULL, 0xD20B5F39B9D3FBDBULL, 0x5579C0BD1A60320AULL,
    0xD6A100C6402C7279ULL, 0x679F25FEFB1FA3CCULL, 0x8EA5E9F8DB3222F8ULL,
    0x3C7516DFFD616B15ULL, 0x2F501EC8AD0552ABULL, 0x323DB5FAFD238760ULL,
    0x53317B483E00DF82ULL, 0x9E5C57BBCA6F8CA0ULL, 0x1A87562EDF1769DBULL,
    0xD542A8F6287EFFC3ULL, 0xAC6732C68C4F5573ULL, 0x695B27B0BBCA58C8ULL,
    0xE1FFA35DB8F011A0ULL, 0x10FA3D98FD2183B8ULL, 0x4AFCB56C2DD1D35BULL,
    0x9A53E479B6F84565ULL, 0xD28E49BC4BFB9790ULL, 0xE1DDF2DAA4CB7E33ULL,
    0x62FB1341CEE4C6E8ULL, 0xEF20CADA36774C01ULL, 0xD07E9EFE2BF11FB4ULL,
    0x95DBDA4DAE909198ULL, 0xEAAD8E716B93D5A0ULL, 0xD08ED1D0AFC725E0ULL,
    0x8E3C5B2F8E7594B7ULL, 0x8FF6E2FBF2122B64ULL, 0x8888B812900DF01CULL,
    0x4FAD5EA0688FC31CULL, 0xD1CFF191B3A8C1ADULL, 0x2F2F2218BE0E1777ULL,
    0xEA752DFE8B021FA1ULL, 0xE5A0CC0FB56F74E8ULL, 0x18ACF3D6CE89E299ULL,
    0xB4A84FE0FD13E0B7ULL, 0x7CC43B81D2ADA8D9ULL, 0x165FA26680957705ULL,
    0x93CC7314211A1477ULL, 0xE6AD206577B5FA86ULL, 0xC75442F5FB9D35CFULL,
    0xEBCDAF0C7B3E89A0ULL, 0xD6411BD3AE1E7E49ULL, 0x00250E2D2071B35EULL,
    0x226800BB57B8E0AFULL, 0x2464369BF009B91EULL, 0x5563911D59DFA6AAULL,
    0x78C14389D95A537FULL, 0x207D5BA202E5B9C5ULL, 0x832603766295CFA9ULL,
    0x11C819684E734A41ULL, 0xB3472DCA7B14A94AULL, 0x1B5100529A532915ULL,
    0xD60F573FBC9BC6E4ULL, 0x2B60A47681E67400ULL, 0x08BA6FB5571BE91FULL,
    0xF296EC6B2A0DD915ULL, 0xB6636521E7B9F9B6ULL, 0xFF34052EC5855664ULL,
    0x53B02D5DA99F8FA1ULL, 0x08BA47996E85076AULL, 0x4B7A70E9B5B32944ULL,
    0xDB75092EC4192623ULL, 0xAD6EA6B049A7DF7DULL, 0x9CEE60B88FEDB266ULL,
    0xECAA8C71699A18FFULL, 0x5664526CC2B19EE1ULL, 0x193602A575094C29ULL,
    0xA0591340E4183A3EULL, 0x3F54989A5B429D65ULL, 0x6B8FE4D699F73FD6ULL,
    0xA1D29C07EFE830F5ULL, 0x4D2D38E6F0255DC1ULL, 0x4CDD20868470EB26ULL,
    0x6382E9C6021ECC5EULL, 0x09686B3F3EBAEFC9ULL, 0x3C9718146B6A70A1ULL,
    0x687F358452A0E286ULL, 0xB79C5305AA500737ULL, 0x3E07841C7FDEAE5CULL,
    0x8E7D44EC5716F2B8ULL, 0xB03ADA37F0500C0DULL, 0xF01C1F040200B3FFULL,
    0xAE0CF51A3CB574B2ULL, 0x25837A58DC0921BDULL, 0xD19113F97CA92FF6ULL,
    0x9432477322F54701ULL, 0x3AE5E58137C2DADCULL, 0xC8B576349AF3DDA7ULL,
    0xA94461460FD0030EULL, 0xECC8C73EA4751E41ULL, 0xE238CD993BEA0E2FULL,
    0x3280BBA1183EB331ULL, 0x4E548B384F6DB908ULL, 0x6F420D03F60A04BFULL,
    0x2CB8129024977C79ULL, 0x5679B072BCAF89AFULL, 0xDE9A771FD9930810ULL,
    0xB38BAE12DCCF3F2EULL, 0x5512721F2E6B7124ULL, 0x501ADDE69F84CD87ULL,
    0x7A5847187408DA17ULL, 0xBC9F9ABCE94B7D8CULL, 0xEC7AEC3ADB851DFAULL,
    0x63094366C464C3D2ULL, 0xEF1C18473215D808ULL, 0xDD433B3724C2BA16ULL,
    0x12A14D432A65C451ULL, 0x50940002133AE4DDULL, 0x71DFF89E10314E55ULL,
    0x81AC77D65F11199BULL, 0x043556F1D7A3C76BULL, 0x3C11183B5924A509ULL,
    0xF28FE6ED97F1FBFAULL, 0x9EBABF2C1E153C6EULL, 0x86E34570EAE96FB1ULL,
    0x860E5E0A5A3E2AB3ULL, 0x771FE71C4E3D06FAULL, 0x2965DCB999E71D0FULL,
    0x803E89D65266C825ULL, 0x2E4CC9789C10B36AULL, 0xC6150EBA94E2EA78ULL,
    0xA6FC3C531E0A2DF4ULL, 0xF2F74EA7361D2B3DULL, 0x1939260F19C27960ULL,
    0x5223A708F71312B6ULL, 0xEBADFE6EEAC31F66ULL, 0xE3BC4595A67BC883ULL,
    0xB17F37D1018CFF28ULL, 0xC332DDEFBE6C5AA5ULL, 0x6558218568AB9702ULL,
    0xEECEA50FDB2F953BULL, 0x2AEF7DAD5B6E2F84ULL, 0x1521B62829076170ULL,
    0xECDD4775619F1510ULL, 0x13CCA830EB61BD96ULL, 0x0334FE1EAA0363CFULL,
    0xB5735C904C70A239ULL, 0xD59E9E0BCBAADE14ULL, 0xEECC86BC60622CA7ULL,
    0x9CAB5CABB2F3846EULL, 0x648B1EAF19BDF0CAULL, 0xA02369B9655ABB50ULL,
    0x40685A323C2AB4B3ULL, 0x319EE9D5C021B8F7ULL, 0x9B540B19875FA099ULL,
    0x95F7997E623D7DA8ULL, 0xF837889A97E32D77ULL, 0x11ED935F16681281ULL,
    0x0E358829C7E61FD6ULL, 0x96DEDFA17858BA99ULL, 0x57F584A51B227263ULL,
    0x9B83C3FF1AC24696ULL, 0xCDB30AEB532E3054ULL, 0x8FD948E46DBC3128ULL,
    0x58EBF2EF34C6FFEAULL, 0xFE28ED61EE7C3C73ULL, 0x5D4A14D9E864B7E3ULL,
    0x42105D14203E13E0ULL, 0x45EEE2B6A3AAABEAULL, 0xDB6C4F15FACB4FD0ULL,
    0xC742F442EF6ABBB5ULL, 0x654F3B1D41CD2105ULL, 0xD81E799E86854DC7ULL,
    0xE44B476A3D816250ULL, 0xCF62A1F25B8D2646ULL, 0xFC8883A0C1C7B6A3ULL,
    0x7F1524C369CB7492ULL, 0x47848A0B5692B285ULL, 0x095BBF00AD19489DULL,
    0x1462B17423820D00ULL, 0x58428D2A0C55F5EAULL, 0x1DADF43E233F7061ULL,
    0x3372F0928D937E41ULL, 0xD65FECF16C223BDBULL, 0x7CDE3759CBEE7460ULL,
    0x4085F2A7CE77326EULL, 0xA607808419F8509EULL, 0xE8EFD85561D99735ULL,
    0xA969A7AAC50C06C2ULL, 0x5A04ABFC800BCADCULL, 0x9E447A2EC3453484ULL,
    0xFDD567050E1E9EC9ULL, 0xDB73DBD3105588CDULL, 0x675FDA79E3674340ULL,
    0xC5C43465713E38D8ULL, 0x3D28F89EF16DFF20ULL, 0x153E21E78FB03D4AULL,
    0xE6E39F2BDB83ADF7ULL, 0xE93D5A68948140F7ULL, 0xF64C261C94692934ULL,
    0x411520F77602D4F7ULL, 0xBCF46B2ED4A10068ULL, 0xD40824713320F46AULL,
    0x43B7D4B7500061AFULL, 0x1E39F62E97244546ULL
};

static inline void
block_shuffle(uint64_t *__restrict__ state)
{
    static const int shuffle[FEISTEL_ROUNDS] = { 7,  2, 13, 4,  11, 8,  3, 6,
                                                 15, 0, 9,  10, 1,  14, 5, 12 };
    uint64_t         source[FEISTEL_BLOCKS * LANES];
    int              branch;

    memcpy(source, state, sizeof source);
    for (branch = 0; branch < FEISTEL_BLOCKS; branch++) {
        const INT128 v = load(source, shuffle[branch]);
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
            const INT128 even = load(state, branch);
            const INT128 odd  = load(state, branch + 1);
            const INT128 f1   = aes(even, load(keys, 0));
            const INT128 f2   = aes(f1, odd);
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
    const int capacity_blocks         = CAPACITY_BYTES / sizeof(INT128);
    int       i;

    for (i = capacity_blocks; i < STATE_BYTES / (int) sizeof(INT128); i++) {
        INT128 block = load(state, i);
        block ^= load(seed, i - capacity_blocks);
        store(block, state, i);
    }
}

static void
generate(void *state_void)
{
    uint64_t *__restrict__ state = (uint64_t *) state_void;
    INT128 prev_inner            = load(state, 0);
    INT128 inner;

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
