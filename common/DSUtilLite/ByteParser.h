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
 *
 *  Initial design and concept by Gabest and the MPC-HC Team, copyright under GPLv2
 */

#pragma once

#include <cstddef>

struct GetBitContext;

/**
 * Byte Parser Utility Class
 */
class CByteParser
{
  public:
    /** Construct a Byte Parser to parse the given BYTE array with the given length */
    CByteParser(const BYTE *pData, std::size_t length);
    virtual ~CByteParser();

    /** Read 1 to 32 Bits from the Byte Array. If peek is set, the data will just be returned, and the buffer not
     * advanced. */
    unsigned int BitRead(unsigned int numBits, bool peek = false);

    /** Skip any number of bits from the byte array */
    void BitSkip(unsigned int numBits);

    /** Read a unsigned number in Exponential Golomb encoding (with k = 0) */
    unsigned int UExpGolombRead();
    /** Read a signed number in Exponential Golomb encoding (with k = 0) */
    int SExpGolombRead();

    /** Pointer to the start of the byte array */
    [[nodiscard]] constexpr const BYTE *Start() const noexcept { return m_pData; }
    /** Pointer to the end of the byte array */
    [[nodiscard]] constexpr const BYTE *End() const noexcept { return m_pEnd; }

    /** Overall length (in bytes) of the byte array */
    std::size_t Length() const;

    std::size_t Pos() const;

    /** Number of bytes remaining in the array */
    [[nodiscard]] std::size_t Remaining() const { return RemainingBits() >> 3; }

    /** Number of bits remaining */
    std::size_t RemainingBits() const;

    void BitByteAlign();

  private:
    GetBitContext *m_gbCtx = nullptr;

    const BYTE *m_pData = nullptr;
    const BYTE *m_pEnd = nullptr;
};
