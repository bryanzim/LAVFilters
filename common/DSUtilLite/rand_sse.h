// Based on rand_sse at
// http://software.intel.com/en-us/articles/fast-random-number-generator-on-the-intel-pentiumr-4-processor/

#include <emmintrin.h>

#include <array>


__declspec(align(16)) static __m128i cur_seed;

void srand_sse(unsigned int seed)
{
    cur_seed = _mm_set_epi32(seed, seed + 1, seed, seed + 1);
}

inline void rand_sse(int *result)
{
    __declspec(align(16)) __m128i cur_seed_split;
    __declspec(align(16)) __m128i multiplier;
    __declspec(align(16)) __m128i adder;
    __declspec(align(16)) __m128i mod_mask;
    __declspec(align(16)) __m128i sra_mask;
    __declspec(align(16)) __m128i sseresult;
    alignas(16) static std::array<const unsigned int, 4U> mult{214013, 17405, 214013, 69069};
    alignas(16) static std::array<const unsigned int, 4U> gadd{2531011, 10395331, 13737667, 1};
    alignas(16) static std::array<const unsigned int, 4U> mask{0xFFFFFFFF, 0, 0xFFFFFFFF, 0};
    alignas(16) static std::array<const unsigned int, 4U> masklo{0x00007FFF, 0x00007FFF, 0x00007FFF, 0x00007FFF};

    adder = _mm_load_si128(reinterpret_cast<const __m128i *>(gadd.data()));
    multiplier = _mm_load_si128(reinterpret_cast<const __m128i *>(mult.data()));
    mod_mask = _mm_load_si128(reinterpret_cast<const __m128i *>(mask.data()));
    sra_mask = _mm_load_si128(reinterpret_cast<const __m128i *>(masklo.data()));
    cur_seed_split = _mm_shuffle_epi32(cur_seed, _MM_SHUFFLE(2, 3, 0, 1));

    cur_seed = _mm_mul_epu32(cur_seed, multiplier);
    multiplier = _mm_shuffle_epi32(multiplier, _MM_SHUFFLE(2, 3, 0, 1));
    cur_seed_split = _mm_mul_epu32(cur_seed_split, multiplier);

    cur_seed = _mm_and_si128(cur_seed, mod_mask);
    cur_seed_split = _mm_and_si128(cur_seed_split, mod_mask);
    cur_seed_split = _mm_shuffle_epi32(cur_seed_split, _MM_SHUFFLE(2, 3, 0, 1));
    cur_seed = _mm_or_si128(cur_seed, cur_seed_split);
    cur_seed = _mm_add_epi32(cur_seed, adder);

    sseresult = _mm_srai_epi32(cur_seed, 16);
    sseresult = _mm_and_si128(sseresult, sra_mask);
    _mm_storeu_si128(reinterpret_cast<__m128i *>(result), sseresult);

    return;
}
