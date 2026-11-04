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

typedef enum
{
    NALU_TYPE_UNKNOWN = 0,
    NALU_TYPE_SLICE = 1,
    NALU_TYPE_DPA = 2,
    NALU_TYPE_DPB = 3,
    NALU_TYPE_DPC = 4,
    NALU_TYPE_IDR = 5,
    NALU_TYPE_SEI = 6,
    NALU_TYPE_SPS = 7,
    NALU_TYPE_PPS = 8,
    NALU_TYPE_AUD = 9,
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12,
    NALU_TYPE_SPS_SUB = 15,
} NALU_TYPE;

class CH264Nalu
{
  protected:
    int forbidden_bit{ 0 };                       //! should be always FALSE
    int nal_reference_idc{ 0 };                   //! NALU_PRIORITY_xxxx
    NALU_TYPE nal_unit_type = NALU_TYPE_UNKNOWN; //! NALU_TYPE_xxxx

    std::size_t m_nNALStartPos{ 0U }; //! NALU start (including startcode / size)
    std::size_t m_nNALDataPos{ 0U };  //! Useful part

    const BYTE *m_pBuffer{ nullptr };
    std::size_t m_nCurPos{ 0U };
    std::size_t m_nNextRTP{ 0U };
    std::size_t m_nSize{ 0U };
    int m_nNALSize{ 0 };

    std::size_t m_nNALHeaderSize{ 1U };

    bool MoveToNextAnnexBStartcode();
    bool MoveToNextRTPStartcode();

  public:
    explicit CH264Nalu() { SetBuffer(nullptr, 0, 0); }
    [[nodiscard]] constexpr NALU_TYPE GetType() const noexcept { return nal_unit_type; }
    [[nodiscard]] bool IsRefFrame() const { return (nal_reference_idc != 0); }

    [[nodiscard]] constexpr std::size_t GetDataLength() const noexcept { return m_nCurPos - m_nNALDataPos; }
    [[nodiscard]] constexpr const BYTE *GetDataBuffer() noexcept { return m_pBuffer + m_nNALDataPos; }
    std::size_t GetRoundedDataLength() const
    {
        std::size_t const nSize = m_nCurPos - m_nNALDataPos;
        return nSize + 128 - (nSize % 128);
    }

    [[nodiscard]] constexpr std::size_t GetLength() const noexcept { return m_nCurPos - m_nNALStartPos; }
    [[nodiscard]] constexpr const BYTE *GetNALBuffer() noexcept { return m_pBuffer + m_nNALStartPos; }
    [[nodiscard]] constexpr std::size_t GetNALPos() noexcept { return m_nNALStartPos; }
    [[nodiscard]] constexpr bool IsEOF() const noexcept { return m_nCurPos >= m_nSize; }

    void SetBuffer(const BYTE *pBuffer, std::size_t nSize, int nNALSize);
    bool ReadNext();
};

class CH265Nalu : public CH264Nalu
{
  public:
    explicit CH265Nalu()
        : CH264Nalu()
    {
        m_nNALHeaderSize = 2U;
    }
    bool ReadNext();
};

class CH264NALUnescape
{
  public:
    CH264NALUnescape(const BYTE *pBuffer, std::size_t nSize);
    ~CH264NALUnescape();
    [[nodiscard]] constexpr const BYTE *GetBuffer() const noexcept { return m_pBuffer; }
    [[nodiscard]] constexpr std::size_t GetSize() const noexcept { return m_nSize; }

  private:
    BYTE *m_pBuffer{ nullptr };
    std::size_t m_nSize{ 0U };
};
