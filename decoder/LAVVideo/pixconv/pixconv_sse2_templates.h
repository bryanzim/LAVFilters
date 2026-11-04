/*
 *      Copyright (C) 2010-2021 Hendrik Leppkes
 *      http://www.1f0.de
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <emmintrin.h>
#include <smmintrin.h>

#include <cstddef>
#include <cstdint>


// Load the dithering coefficients for this line
// reg   - register to load coefficients into
// line  - index of line to process (0 based)
// bits  - number of bits to dither (for 10 -> 8, set to 2)
#define PIXCONV_LOAD_DITHER_COEFFS(reg, line, bits, name) \
    const std::uint16_t *name = dither_8x8_256[(line) % 8].data();    \
    reg = _mm_load_si128(reinterpret_cast<const __m128i *>(name));          \
    reg = _mm_srli_epi16(reg, 8 - bits); /* shift to the required dithering strength */

// Load 8 16-bit pixels into a register, using aligned memory access
// reg   - register to store pixels in
// src   - memory pointer of the source
// bpp   - bit depth of the pixels
#define PIXCONV_LOAD_PIXEL16(reg, src, bpp)                            \
    reg = _mm_load_si128(reinterpret_cast<const __m128i *>((src))); /* load (aligned) */ \
    reg = _mm_slli_epi16(reg, 16 - bpp);          /* shift to 16-bit */

// Load 2x8 16-bit pixels into registers, using aligned memory access
// reg1   - register to store pixels in
// reg2   - register to store pixels in
// src1   - memory pointer of the source
// src2   - memory pointer of the source
// bpp   - bit depth of the pixels
#define PIXCONV_LOAD_PIXEL16X2(reg1, reg2, src1, src2, bpp) \
    {                                                       \
        const __m128i shift = _mm_cvtsi32_si128(16 - bpp);  \
        reg1 = _mm_load_si128(reinterpret_cast<const __m128i *>((src1)));     \
        reg2 = _mm_load_si128(reinterpret_cast<const __m128i *>((src2)));     \
        reg1 = _mm_sll_epi16(reg1, shift);                  \
        reg2 = _mm_sll_epi16(reg2, shift);                  \
    }

// Load 8 16-bit pixels into a register, and dither them to 8 bit
// The 8-bit pixels will be in the high-bytes of the 8 16-bit parts
// NOTE: the low-bytes are clobbered, and not empty.
// reg   - register to store pixels in
// dreg  - register with dithering coefficients
// src   - memory pointer of the source
// bpp   - bit depth of the pixels
#define PIXCONV_LOAD_PIXEL16_DITHER_HIGH(reg, dreg, src, bpp) \
    PIXCONV_LOAD_PIXEL16(reg, src, bpp)                       \
    reg = _mm_adds_epu16(reg, dreg); /* dither */

// Load 8 16-bit pixels into a register, and dither them to 8 bit
// The 8-bit pixels will be in the low-bytes of the 8 16-bit parts
// reg   - register to store pixels in
// dreg  - register with dithering coefficients
// src   - memory pointer of the source
// bpp   - bit depth of the pixels
#define PIXCONV_LOAD_PIXEL16_DITHER(reg, dreg, src, bpp)  \
    PIXCONV_LOAD_PIXEL16_DITHER_HIGH(reg, dreg, src, bpp) \
    reg = _mm_srli_epi16(reg, 8); /* shift to 8-bit */

// Load 8 16-bit pixels into a register, and dither them to 8 bit
// The 8-bit pixels will be in the 8 low-bytes in the register
// reg   - register to store pixels in
// dreg  - register with dithering coefficients
// src   - memory pointer of the source
// bpp   - bit depth of the pixels
#define PIXCONV_LOAD_PIXEL16_DITHER_PACKED(reg, dreg, zero, src, bpp)    \
    PIXCONV_LOAD_PIXEL16_DITHER(reg, dreg, src, bpp) /* load unpacked */ \
    reg = _mm_packus_epi16(reg, zero);               /* pack */

// Load 16 8-bit pixels into a register
// reg   - register to store pixels in
// src   - memory pointer of the source
#define PIXCONV_LOAD_PIXEL8(reg, src) reg = _mm_loadu_si128((const __m128i *)(src)); /* load (unaligned) */

// Load 128-bit into a register, using aligned memory access
// reg   - register to store pixels in
// src   - memory pointer of the source
#define PIXCONV_LOAD_ALIGNED(reg, src) reg = _mm_load_si128((const __m128i *)(src)); /* load (aligned) */

// Load 128-bit into a register, using streaming  memory access
// reg   - register to store pixels in
// src   - memory pointer of the source
#define PIXCONV_STREAM_LOAD(reg, src) reg = _mm_stream_load_si128((__m128i *)(src)); /* load (streaming) */

#define PIXCONV_LOAD_PIXEL8_ALIGNED PIXCONV_LOAD_ALIGNED

// Put 128-bit into memory, using streaming write
#define PIXCONV_PUT_STREAM(dst, reg) _mm_stream_si128((__m128i *)(dst), reg); /* streaming write */

// Load 4 8-bit pixels into the register
// reg     - register to store pixels in
// src     - source memory
#define PIXCONV_LOAD_4PIXEL8(reg, src) reg = _mm_cvtsi32_si128(*(const int *)(src)); /* load 32-bit (4 pixel) */

// Load 4 16-bit pixels into the register
// reg     - register to store pixels in
// src     - source memory
#define PIXCONV_LOAD_4PIXEL16(reg, src) reg = _mm_loadl_epi64((const __m128i *)(src)); /* load 64-bit (4 pixel) */

// SSE2 Aligned memcpy
// dst - memory destination
// src - memory source
// len - size in bytes
#define PIXCONV_MEMCPY_ALIGNED(dst, src, len)              \
    {                                                      \
        const std::uint8_t *const srcLinePtr = (src);           \
        std::uint8_t *const dstLinePtr = (dst);                 \
        __m128i r1, r2, r3, r4;                            \
        std::ptrdiff_t copyOffset;                              \
        for (copyOffset = 0; copyOffset < (len - 63); copyOffset += 64) \
        {                                                  \
            PIXCONV_LOAD_ALIGNED(r1, srcLinePtr + copyOffset + 0)   \
            PIXCONV_LOAD_ALIGNED(r2, srcLinePtr + copyOffset + 16); \
            PIXCONV_LOAD_ALIGNED(r3, srcLinePtr + copyOffset + 32); \
            PIXCONV_LOAD_ALIGNED(r4, srcLinePtr + copyOffset + 48); \
            PIXCONV_PUT_STREAM(dstLinePtr + copyOffset + 0, r1);    \
            PIXCONV_PUT_STREAM(dstLinePtr + copyOffset + 16, r2);   \
            PIXCONV_PUT_STREAM(dstLinePtr + copyOffset + 32, r3);   \
            PIXCONV_PUT_STREAM(dstLinePtr + copyOffset + 48, r4);   \
        }                                                  \
        for (; copyOffset < len; copyOffset += 16)         \
        {                                                  \
            PIXCONV_LOAD_ALIGNED(r1, srcLinePtr + copyOffset);      \
            PIXCONV_PUT_STREAM(dstLinePtr + copyOffset, r1);        \
        }                                                  \
    }

// SSE2 Aligned memcpy
// Copys the same size from two source into two destinations at the same time
// dst1 - memory destination
// src1 - memory source
// dst2 - memory destination
// src2 - memory source
// len  - size in bytes
#define PIXCONV_MEMCPY_ALIGNED_TWO(dst1, src1, dst2, src2, len) \
    {                                                           \
        const std::uint8_t *const src1LinePtr = (src1);              \
        const std::uint8_t *const src2LinePtr = (src2);              \
        std::uint8_t *const dst1LinePtr = (dst1);                    \
        std::uint8_t *const dst2LinePtr = (dst2);                    \
        __m128i r1, r2, r3, r4, r5, r6, r7, r8;                 \
        std::ptrdiff_t copyOffset;                                   \
        for (copyOffset = 0; copyOffset < (len - 63); copyOffset += 64) \
        {                                                       \
            PIXCONV_LOAD_ALIGNED(r1, src1LinePtr + copyOffset + 0);      \
            PIXCONV_LOAD_ALIGNED(r2, src1LinePtr + copyOffset + 16);     \
            PIXCONV_LOAD_ALIGNED(r3, src1LinePtr + copyOffset + 32);     \
            PIXCONV_LOAD_ALIGNED(r4, src1LinePtr + copyOffset + 48);     \
            PIXCONV_LOAD_ALIGNED(r5, src2LinePtr + copyOffset + 0);      \
            PIXCONV_LOAD_ALIGNED(r6, src2LinePtr + copyOffset + 16);     \
            PIXCONV_LOAD_ALIGNED(r7, src2LinePtr + copyOffset + 32);     \
            PIXCONV_LOAD_ALIGNED(r8, src2LinePtr + copyOffset + 48);     \
            PIXCONV_PUT_STREAM(dst1LinePtr + copyOffset + 0, r1);        \
            PIXCONV_PUT_STREAM(dst1LinePtr + copyOffset + 16, r2);       \
            PIXCONV_PUT_STREAM(dst1LinePtr + copyOffset + 32, r3);       \
            PIXCONV_PUT_STREAM(dst1LinePtr + copyOffset + 48, r4);       \
            PIXCONV_PUT_STREAM(dst2LinePtr + copyOffset + 0, r5);        \
            PIXCONV_PUT_STREAM(dst2LinePtr + copyOffset + 16, r6);       \
            PIXCONV_PUT_STREAM(dst2LinePtr + copyOffset + 32, r7);       \
            PIXCONV_PUT_STREAM(dst2LinePtr + copyOffset + 48, r8);       \
        }                                                       \
        for (; copyOffset < len; copyOffset += 16)              \
        {                                                       \
            PIXCONV_LOAD_ALIGNED(r1, src1LinePtr + copyOffset);          \
            PIXCONV_LOAD_ALIGNED(r2, src2LinePtr + copyOffset);          \
            PIXCONV_PUT_STREAM(dst1LinePtr + copyOffset, r1);            \
            PIXCONV_PUT_STREAM(dst2LinePtr + copyOffset, r2);            \
        }                                                       \
    }
