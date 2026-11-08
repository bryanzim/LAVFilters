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
 * This code was inspired by the ffdshow-tryouts TaudioCodecBitstream module, licensed under GPL 2.0
 */

#include "stdafx.h"

#include "LAVAudio.h"

#include <array>
#include <cstdint>
#include <cstring>


#pragma warning(push)
#pragma warning(disable : 4101)
#pragma warning(disable : 5033)
extern "C"
{
#define AVCODEC_X86_MATHOPS_H
#include "libavcodec/get_bits.h"
#include "libavformat/spdif.h"
}
#pragma warning(pop)

#define MAT_BUFFER_SIZE (61440)
#define MAT_BUFFER_LIMIT (MAT_BUFFER_SIZE - 24 /* MAT end code size */)
#define MAT_POS_MIDDLE (30708 /* middle point*/ + 8 /* IEC header in front */)

static std::array<const BYTE, 20U> mat_start_code{0x07, 0x9E, 0x00, 0x03, 0x84, 0x01, 0x01, 0x01, 0x80, 0x00,
                                        0x56, 0xA5, 0x3B, 0xF4, 0x81, 0x83, 0x49, 0x80, 0x77, 0xE0};
static std::array<const BYTE, 12U> mat_middle_code{0xC3, 0xC1, 0x42, 0x49, 0x3B, 0xFA, 0x82, 0x83, 0x49, 0x80, 0x77, 0xE0};
static std::array<const BYTE, 24U> mat_end_code{0xC3, 0xC2, 0xC0, 0xC4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                      0x00, 0x00, 0x97, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

void CLAVAudio::MATWriteHeader()
{
    ASSERT(m_bsOutput.GetCount() == 0);

    DWORD const dwSize = BURST_HEADER_SIZE + sizeof(mat_start_code);

    // reserve size for the IEC header and the MAT start code
    if (FAILED(m_bsOutput.SetSize(dwSize)))
    {
        return;
    }

    BYTE *p = m_bsOutput.Ptr();

    // IEC burst header
    AV_WB16(p + 0, SYNCWORD1);
    AV_WB16(p + 2, SYNCWORD2);
    AV_WB16(p + 4, IEC61937_TRUEHD);
    AV_WB16(p + 6, 61424);

    // MAT start code
    std::memcpy(p + BURST_HEADER_SIZE, mat_start_code.data(), sizeof(mat_start_code));

    // unless the start code falls into the padding,  its considered part of the current MAT frame
    // Note that audio frames are not always aligned with MAT frames, so we might already have a partial frame at this
    // point
    m_TrueHDMATState.mat_framesize += dwSize;

    // The MAT metadata counts as padding, if we're scheduled to write any, which mean the start bytes should reduce any
    // further padding.
    if (m_TrueHDMATState.padding > 0)
    {
        // if the header fits into the padding of the last frame, just reduce the amount of needed padding
        if (m_TrueHDMATState.padding > dwSize)
        {
            m_TrueHDMATState.padding -= dwSize;
            m_TrueHDMATState.mat_framesize = 0;
        }
        else // otherwise, consume all padding and set the size of the next MAT frame to the remaining data
        {
            m_TrueHDMATState.mat_framesize = (dwSize - m_TrueHDMATState.padding);
            m_TrueHDMATState.padding = 0;
        }
    }
}

void CLAVAudio::MATWritePadding()
{
    if (m_TrueHDMATState.padding > 0)
    {
        int const remaining = MATFillDataBuffer(nullptr, m_TrueHDMATState.padding, true);

        // not all padding could be written to the buffer, write it later
        if (remaining >= 0)
        {
            m_TrueHDMATState.padding = remaining;
            m_TrueHDMATState.mat_framesize = 0;
        }
        else // more padding then requested was written, eg. there was a MAT middle/end marker that needed to be written
        {
            m_TrueHDMATState.padding = 0;
            m_TrueHDMATState.mat_framesize = -remaining;
        }
    }
}

void CLAVAudio::MATAppendData(const BYTE * const p, int size)
{
    HRESULT const hr = p ? m_bsOutput.Append(p, static_cast<DWORD>(size)) : m_bsOutput.AppendZero(static_cast<DWORD>(size));
    if (FAILED(hr))
    {
        return;
    }

    m_TrueHDMATState.mat_framesize += size;
}

int CLAVAudio::MATFillDataBuffer(const BYTE *const p, int size, bool padding)
{
    ASSERT((p || padding));

    if (m_bsOutput.GetCount() >= MAT_BUFFER_LIMIT) {
        return size;
    }

    int remaining = size;

    // Write MAT middle marker, if needed
    // The MAT middle marker always needs to be in the exact same spot, any audio data will be split.
    // If we're currently writing padding, then the marker will be considered as padding data and reduce the amount of
    // padding still required.
    if ((m_bsOutput.GetCount() <= MAT_POS_MIDDLE) && (m_bsOutput.GetCount() + size > MAT_POS_MIDDLE))
    {
        // write as much data before the middle code as we can
        int const nBytesBefore = MAT_POS_MIDDLE - m_bsOutput.GetCount();
        MATAppendData(p, nBytesBefore);
        remaining -= nBytesBefore;

        // write the MAT middle code
        MATAppendData(mat_middle_code.data(), sizeof(mat_middle_code));

        // if we're writing padding, deduct the size of the code from it
        if (padding) {
            remaining -= sizeof(mat_middle_code);
        }

        // write remaining data after the MAT marker
        if (remaining > 0)
        {
            return MATFillDataBuffer(p ? p + nBytesBefore : nullptr, remaining, padding);
        }

        return remaining;
    }

    // not enough room in the buffer to write all the data, write as much as we can and add the MAT footer
    if (m_bsOutput.GetCount() + size >= MAT_BUFFER_LIMIT)
    {
        // write as much data before the middle code as we can
        int const nBytesBefore = MAT_BUFFER_LIMIT - m_bsOutput.GetCount();
        MATAppendData(p, nBytesBefore);
        remaining -= nBytesBefore;

        // write the MAT end code
        MATAppendData(mat_end_code.data(), sizeof(mat_end_code));

        ASSERT(m_bsOutput.GetCount() == MAT_BUFFER_SIZE);

        // MAT markers don't displace padding, so reduce the amount of padding
        if (padding) {
            remaining -= sizeof(mat_end_code);
        }

        // any remaining data will be written in future calls
        return remaining;
    }

    MATAppendData(p, size);

    return 0;
}

void CLAVAudio::MATFlushPacket(HRESULT *hrDeliver)
{
    if (m_bsOutput.GetCount() > 0)
    {
        ASSERT(m_bsOutput.GetCount() == 61440);

        // normal number of samples per frame
        std::uint16_t const frame_samples = 40 << (m_TrueHDMATState.ratebits & 7);
        int const nMATSamples = (frame_samples * 24);

        // Deliver MAT packet to the audio renderer
        *hrDeliver = DeliverBitstream(m_nCodecId, m_bsOutput.Ptr(), m_bsOutput.GetCount(), m_rtStartInputCache,
                                      m_rtStopInputCache, true, m_TrueHDMATState.nSamplesOffset);

        // we expect 24 frames per MAT frame, so calculate an offset from that
        // this is done after delivery, because it modifies the duration of the frame, eg. the start of the next frame
        if (static_cast<DWORD>(nMATSamples) != m_TrueHDMATState.nSamples) {
            m_TrueHDMATState.nSamplesOffset += m_TrueHDMATState.nSamples - nMATSamples;
        }

        if (FAILED(m_bsOutput.SetSize(0)) && SUCCEEDED(*hrDeliver))
        {
            *hrDeliver = E_OUTOFMEMORY;
        }
        m_TrueHDMATState.nSamples = 0;
    }
}

static bool ParseTrueHDMajorSyncHeaders(const BYTE *p, int buffsize, int &ratebits, std::uint16_t &output_timing,
                                        bool &output_timing_present)
{
    ASSERT(AV_RB32(p + 4) == 0xf8726fba);

    if (buffsize < 32) { return false; }

    // parse major sync and look for a restart header
    int major_sync_size{ 28 };
    if (p[29U] & 1)
    {
        int const extension_size = p[30U] >> 4;
        major_sync_size += 2 + extension_size * 2;
    }

    GetBitContext gb;
    init_get_bits8(&gb, p + 4, buffsize - 4);
    skip_bits_long(&gb, 32); // format_sync

    // v(32) format_info
    ratebits = get_bits(&gb, 4); // ratebits
    skip_bits1(&gb);             // 6ch_multichannel_type
    skip_bits1(&gb);             // 8ch_multichannel_type
    skip_bits(&gb, 2);           // reserved

    skip_bits(&gb, 2);  // 2ch_presentation_channel_modifier
    skip_bits(&gb, 2);  // 6ch_presentation_channel_modifier
    skip_bits(&gb, 5);  // 6ch_presentation_channel_assignment
    skip_bits(&gb, 2);  // 8ch_presentation_channel_modifier
    skip_bits(&gb, 13); // 8ch_presentation_channel_assignment

    skip_bits(&gb, 16); // signature
    skip_bits(&gb, 16); // flags
    skip_bits(&gb, 16); // reserved

    skip_bits1(&gb);    // variable_rate
    skip_bits(&gb, 15); // peak_data_rate

    int const num_substreams = get_bits(&gb, 4);
    skip_bits_long(&gb, 4 + (major_sync_size - 17) * 8);

    // substream directory
    for (int i{ 0 }; i < num_substreams; ++i)
    {
        int const extra_substream_word = get_bits1(&gb);
        skip_bits1(&gb);    // restart_nonexistent
        skip_bits1(&gb);    // crc_present
        skip_bits1(&gb);    // reserved
        skip_bits(&gb, 12); // substream_end_ptr
        if (extra_substream_word) {
            skip_bits(&gb, 16); // drc_gain_update, drc_time_update, reserved
        }
    }

    // substream segments
    for (int i{ 0 }; i < num_substreams; ++i)
    {
        if (get_bits1(&gb))
        { // block_header_exists
            if (get_bits1(&gb))
            {                       // restart_header_exists
                skip_bits(&gb, 14); // restart_sync_word
                output_timing = static_cast<std::uint16_t>(get_bits(&gb, 16));
                output_timing_present = true;
                // XXX: restart header
            }
            // XXX: Block header
        }
        // XXX: All blocks, all substreams?
        break;
    }

    return true;
}

// output_timing leads the input timing by one access unit. The delta is a signed
// 16-bit sample count, the same conversion the FFmpeg TrueHD IEC 61937 muxer uses.
[[nodiscard]] constexpr int TrueHDOutputInputOffset(std::uint16_t const outputTiming, std::uint16_t const frameSamples,
                                                    std::uint16_t const frameTime) noexcept
{
    std::uint16_t const previousOutput = static_cast<std::uint16_t>(outputTiming - frameSamples);
    return static_cast<std::int16_t>(static_cast<std::uint16_t>(previousOutput - frameTime));
}

HRESULT CLAVAudio::BitstreamTrueHD(const BYTE *p, int buffsize, HRESULT *hrDeliver)
{
    // On a high level, a MAT frame consists of a sequence of padded TrueHD frames
    // The size of the padded frame can be determined from the frame time/sequence code in the frame header,
    // since it varies to accommodate spikes in bitrate.
    // In average all frames are always padded to 2560 bytes, so that 24 frames fit in one MAT frame, however
    // due to bitrate spikes single sync frames have been observed to use up to twice that size, in which
    // case they'll be preceded by smaller frames to keep the average bitrate constant.
    // A constant padding to 2560 bytes can work (this is how the ffmpeg spdifenc module works), however
    // high-bitrate streams can overshoot this size and therefor require proper handling of dynamic padding.

    std::uint16_t output_timing{ 0U };
    bool bOutputTimingPresent{ false };

    // get the ratebits and output timing from the sync frame
    if (AV_RB32(p + 4) == 0xf8726fba)
    {
        if (ParseTrueHDMajorSyncHeaders(p, buffsize, m_TrueHDMATState.ratebits, output_timing, bOutputTimingPresent) ==
            false) {
            return E_FAIL;
        }
    }
    else if (m_TrueHDMATState.prev_frametime_valid == false)
    {
        // only start streaming on a major sync frame
        m_rtBitstreamCache = AV_NOPTS_VALUE;
        m_TrueHDMATState.nSamplesOffset = 0;
        return S_FALSE;
    }

    std::uint16_t const frame_time = AV_RB16(p + 2);
    std::uint32_t space_size{ 0U };

    int const rateShift = m_TrueHDMATState.ratebits & 7;
    std::uint16_t const frame_samples = static_cast<std::uint16_t>(40U << rateShift);
    int const bytesPerSample = 64 >> rateShift;
    constexpr int kTrueHDMaxPadding{ 61424 * 2 };
    bool discontinuity{ false };

    m_TrueHDMATState.output_timing += frame_samples;
    if (bOutputTimingPresent)
    {
        if (m_TrueHDMATState.output_timing_valid && (output_timing != m_TrueHDMATState.output_timing))
        {
            discontinuity = true;
            m_TrueHDMATState.prev_frametime_valid = false;

            int const currentOffset = TrueHDOutputInputOffset(output_timing, frame_samples, frame_time);
            int const nominalSpacing = static_cast<int>(frame_samples) * bytesPerSample;
            int const previousPadding = (m_TrueHDMATState.prev_mat_framesize != 0U)
                                             ? (nominalSpacing - static_cast<int>(m_TrueHDMATState.prev_mat_framesize))
                                             : 0;
            int const totalPadding =
                previousPadding + ((m_TrueHDMATState.nOutputTimeOffset - currentOffset) * bytesPerSample);

            // A negative or huge correction would insert seconds of silence, or wrap the DWORD pad.
            if ((totalPadding < 0) || (totalPadding > kTrueHDMaxPadding))
            {
                DbgLog((LOG_TRACE, 10, _T("BitstreamTrueHD(): Ignoring discontinuity padding %d (offset %d -> %d)"),
                        totalPadding, m_TrueHDMATState.nOutputTimeOffset, currentOffset));
            }
            else
            {
                m_TrueHDMATState.padding += static_cast<DWORD>(totalPadding);
                DbgLog((LOG_TRACE, 10, _T("BitstreamTrueHD(): Carrying forward %d padding (offset %d -> %d)"),
                        totalPadding, m_TrueHDMATState.nOutputTimeOffset, currentOffset));
            }
        }
        m_TrueHDMATState.output_timing = output_timing;
        m_TrueHDMATState.output_timing_valid = true;
    }

    // The discontinuity path already includes the nominal gap. Adding it again would double the pad.
    if (!discontinuity)
    {
        // compute final padded size for the previous frame, if any
        if (m_TrueHDMATState.prev_frametime_valid)
        {
            std::uint16_t const sampleDelta = static_cast<std::uint16_t>(frame_time - m_TrueHDMATState.prev_frametime);
            space_size = static_cast<std::uint32_t>(sampleDelta) * static_cast<std::uint32_t>(bytesPerSample);
        }

        // compute padding (ie. difference to the size of the previous frame)
        ASSERT((!m_TrueHDMATState.prev_frametime_valid || (space_size >= m_TrueHDMATState.prev_mat_framesize)));

        // if for some reason the space_size fails, align the actual frame size
        if ((space_size < m_TrueHDMATState.prev_mat_framesize) && (bytesPerSample > 0))
        {
            space_size = FFALIGN(m_TrueHDMATState.prev_mat_framesize, bytesPerSample);
        }

        int const framePadding = static_cast<int>(space_size) - static_cast<int>(m_TrueHDMATState.prev_mat_framesize);
        if ((framePadding > 0) && (framePadding <= kTrueHDMaxPadding))
        {
            m_TrueHDMATState.padding += static_cast<DWORD>(framePadding);
        }
    }

    // Signed 16-bit output-minus-input delta, including a negative correction the next discontinuity can apply.
    if (m_TrueHDMATState.output_timing_valid)
    {
        m_TrueHDMATState.nOutputTimeOffset =
            TrueHDOutputInputOffset(m_TrueHDMATState.output_timing, frame_samples, frame_time);
    }

    // store frame time of the previous frame
    m_TrueHDMATState.prev_frametime = frame_time;
    m_TrueHDMATState.prev_frametime_valid = true;

    // Write the MAT header into the fresh buffer
    if (m_bsOutput.GetCount() == 0)
    {
        MATWriteHeader();

        // initial header, don't count it for the frame size
        if (m_TrueHDMATState.init == false)
        {
            m_TrueHDMATState.init = true;
            m_TrueHDMATState.mat_framesize = 0;
        }
    }

    // write padding of the previous frame (if any)
    while (m_TrueHDMATState.padding > 0)
    {
        MATWritePadding();

        ASSERT(((m_TrueHDMATState.padding == 0) || (m_bsOutput.GetCount() == MAT_BUFFER_SIZE)));

        // Buffer is full, submit it
        if (m_bsOutput.GetCount() == MAT_BUFFER_SIZE)
        {
            MATFlushPacket(hrDeliver);

            // and setup a new buffer
            MATWriteHeader();
        }
    }

    // count the number of samples in this frame
    m_TrueHDMATState.nSamples += frame_samples;

    // write actual audio data to the buffer
    int remaining = MATFillDataBuffer(p, buffsize);

    // not all data could be written, or the buffer is full
    if (remaining || (m_bsOutput.GetCount() == MAT_BUFFER_SIZE))
    {
        // flush out old data
        MATFlushPacket(hrDeliver);

        if (remaining)
        {
            // .. setup a new buffer
            MATWriteHeader();

            // and write the remaining data
            remaining = MATFillDataBuffer(p + (buffsize - remaining), remaining);

            ASSERT(remaining == 0);
        }
    }

    // store the size of the current MAT frame, so we can add padding later
    m_TrueHDMATState.prev_mat_framesize = m_TrueHDMATState.mat_framesize;
    m_TrueHDMATState.mat_framesize = 0;

    return S_OK;
}
