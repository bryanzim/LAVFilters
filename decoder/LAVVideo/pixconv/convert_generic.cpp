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

#include "stdafx.h"
#include "pixconv_internal.h"

extern "C"
{
#include "libavutil/intreadwrite.h"
};

#define ALIGN(x, a) (((x) + (a)-1UL) & ~((a)-1UL))

DECLARE_CONV_FUNC_IMPL(convert_generic)
{
    HRESULT hr = S_OK;

    AVPixelFormat const inputFmt = GetFFInput();

    switch (m_OutputPixFmt)
    {
    case LAVOutPixFmt_YV12:
        hr = swscale_scale(inputFmt, AV_PIX_FMT_YUV420P, src, srcStride, dst, width, height, dstStride,
                           lav_pixfmt_desc[m_OutputPixFmt], true);
        break;
    case LAVOutPixFmt_NV12:
        hr = swscale_scale(inputFmt, AV_PIX_FMT_NV12, src, srcStride, dst, width, height, dstStride,
                           lav_pixfmt_desc[m_OutputPixFmt]);
        break;
    case LAVOutPixFmt_YUY2: hr = ConvertTo422Packed(src, srcStride, dst, width, height, dstStride); break;
    case LAVOutPixFmt_UYVY: hr = ConvertTo422Packed(src, srcStride, dst, width, height, dstStride); break;
    case LAVOutPixFmt_AYUV: hr = ConvertToAYUV(src, srcStride, dst, width, height, dstStride); break;
    case LAVOutPixFmt_P010: hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 2); break;
    case LAVOutPixFmt_P016: hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 2); break;
    case LAVOutPixFmt_P210: hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 1); break;
    case LAVOutPixFmt_P216: hr = ConvertToPX1X(src, srcStride, dst, width, height, dstStride, 1); break;
    case LAVOutPixFmt_Y410: hr = ConvertToY410(src, srcStride, dst, width, height, dstStride); break;
    case LAVOutPixFmt_Y416: hr = ConvertToY416(src, srcStride, dst, width, height, dstStride); break;
    case LAVOutPixFmt_RGB32:
        hr = swscale_scale(inputFmt, AV_PIX_FMT_BGRA, src, srcStride, dst, width, height, dstStride,
                           lav_pixfmt_desc[m_OutputPixFmt]);
        break;
    case LAVOutPixFmt_RGB24:
        hr = swscale_scale(inputFmt, AV_PIX_FMT_BGR24, src, srcStride, dst, width, height, dstStride,
                           lav_pixfmt_desc[m_OutputPixFmt]);
        break;
    case LAVOutPixFmt_v210: hr = ConvertTov210(src, srcStride, dst, width, height, dstStride); break;
    case LAVOutPixFmt_v410: hr = ConvertTov410(src, srcStride, dst, width, height, dstStride); break;
    case LAVOutPixFmt_YV16:
        hr = swscale_scale(inputFmt, AV_PIX_FMT_YUV422P, src, srcStride, dst, width, height, dstStride,
                           lav_pixfmt_desc[m_OutputPixFmt], true);
        break;
    case LAVOutPixFmt_YV24:
        hr = swscale_scale(inputFmt, AV_PIX_FMT_YUV444P, src, srcStride, dst, width, height, dstStride,
                           lav_pixfmt_desc[m_OutputPixFmt], true);
        break;
    case LAVOutPixFmt_RGB48:
        hr = swscale_scale(inputFmt, AV_PIX_FMT_RGB48LE, src, srcStride, dst, width, height, dstStride,
                           lav_pixfmt_desc[m_OutputPixFmt]);
        break;
    default:
        ASSERT(0);
        hr = E_FAIL;
        break;
    }

    return S_OK;
}

inline SwsContext *CLAVPixFmtConverter::GetSWSContext(int width, int height, enum AVPixelFormat srcPix,
                                                      enum AVPixelFormat dstPix, int flags)
{
    if (!m_pSwsContext || swsWidth != width || swsHeight != height)
    {
        // Get context
        m_pSwsContext = sws_getCachedContext(m_pSwsContext, width, height, srcPix, width, height, dstPix,
                                             flags | SWS_PRINT_INFO, nullptr, nullptr, nullptr);

        int *inv_tbl = nullptr, *tbl = nullptr;
        int srcRange, dstRange, brightness, contrast, saturation;
        int const ret = sws_getColorspaceDetails(m_pSwsContext, &inv_tbl, &srcRange, &tbl, &dstRange, &brightness, &contrast,
                                           &saturation);
        if (ret >= 0)
        {
            const int *rgbTbl = nullptr;
            if (m_ColorProps.VideoTransferMatrix != DXVA2_VideoTransferMatrix_Unknown)
            {
                int colorspace = SWS_CS_ITU709;
                switch (m_ColorProps.VideoTransferMatrix)
                {
                case DXVA2_VideoTransferMatrix_BT709: colorspace = SWS_CS_ITU709; break;
                case DXVA2_VideoTransferMatrix_BT601: colorspace = SWS_CS_ITU601; break;
                case DXVA2_VideoTransferMatrix_SMPTE240M: colorspace = SWS_CS_SMPTE240M; break;
                }
                rgbTbl = sws_getCoefficients(colorspace);
            }
            else
            {
                BOOL const isHD = (height >= 720 || width >= 1280);
                rgbTbl = sws_getCoefficients(isHD ? SWS_CS_ITU709 : SWS_CS_ITU601);
            }
            srcRange = dstRange = (m_ColorProps.NominalRange == DXVA2_NominalRange_0_255);
            sws_setColorspaceDetails(m_pSwsContext, rgbTbl, srcRange, rgbTbl, dstRange, brightness, contrast,
                                     saturation);
        }
        swsWidth = width;
        swsHeight = height;
    }
    return m_pSwsContext;
}

HRESULT CLAVPixFmtConverter::swscale_scale(enum AVPixelFormat srcPix, enum AVPixelFormat dstPix,
                                           const uint8_t *const src[], const ptrdiff_t srcStride[], uint8_t *dst[],
                                           int width, int height, const ptrdiff_t dstStride[],
                                           LAVOutPixFmtDesc pixFmtDesc, bool swapPlanes12)
{
    int ret;

    SwsContext *ctx = GetSWSContext(width, height, srcPix, dstPix, SWS_BILINEAR);
    CheckPointer(m_pSwsContext, E_POINTER);

    if (swapPlanes12)
    {
        uint8_t *tmp = dst[1U];
        dst[1U] = dst[2U];
        dst[2U] = tmp;
    }

    ret = sws_scale2(ctx, src, srcStride, 0, height, dst, dstStride);

    return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertTo422Packed(const uint8_t *const src[4U], const ptrdiff_t srcStride[4U],
                                                uint8_t *dst[], int width, int height, const ptrdiff_t dstStride[])
{
    const BYTE *y = nullptr;
    const BYTE *u = nullptr;
    const BYTE *v = nullptr;
    ptrdiff_t line, i;
    ptrdiff_t sourceStride = 0;
    BYTE *pTmpBuffer = nullptr;

    if (m_InputPixFmt != LAVPixFmt_YUV422)
    {
        uint8_t *tmp[4U] = {nullptr};
        ptrdiff_t tmpStride[4U] = {0};
        ptrdiff_t const scaleStride = FFALIGN(width, 32);

        pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 2);
        if (pTmpBuffer == nullptr) {
            return E_OUTOFMEMORY;
}

        tmp[0U] = pTmpBuffer;
        tmp[1U] = tmp[0U] + (height * scaleStride);
        tmp[2U] = tmp[1U] + (height * scaleStride / 2);
        tmp[3U] = nullptr;

        tmpStride[0U] = scaleStride;
        tmpStride[1U] = scaleStride / 2;
        tmpStride[2U] = scaleStride / 2;
        tmpStride[3U] = 0;

        SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), AV_PIX_FMT_YUV422P, SWS_BILINEAR);
        sws_scale2(ctx, src, srcStride, 0, height, tmp, tmpStride);

        y = tmp[0U];
        u = tmp[1U];
        v = tmp[2U];
        sourceStride = scaleStride;
    }
    else
    {
        y = src[0U];
        u = src[1U];
        v = src[2U];
        sourceStride = srcStride[0U];
    }

#define YUV422_PACK_YUY2(offset) \
    *idst++ = y[(i + offset) * 2] | (u[i + offset] << 8) | (y[(i + offset) * 2 + 1] << 16) | (v[i + offset] << 24);
#define YUV422_PACK_UYVY(offset) \
    *idst++ = u[i + offset] | (y[(i + offset) * 2] << 8) | (v[i + offset] << 16) | (y[(i + offset) * 2 + 1] << 24);

    uint8_t *out = dst[0U];
    int const halfwidth = width >> 1;
    ptrdiff_t const halfstride = sourceStride >> 1;

    if (m_OutputPixFmt == LAVOutPixFmt_YUY2)
    {
        for (line = 0; line < height; ++line)
        {
            uint32_t *idst = (uint32_t *)out;
            for (i = 0; i < (halfwidth - 7); i += 8)
            {
                YUV422_PACK_YUY2(0)
                YUV422_PACK_YUY2(1)
                YUV422_PACK_YUY2(2)
                YUV422_PACK_YUY2(3)
                YUV422_PACK_YUY2(4)
                YUV422_PACK_YUY2(5)
                YUV422_PACK_YUY2(6)
                YUV422_PACK_YUY2(7)
            }
            for (; i < halfwidth; ++i)
            {
                YUV422_PACK_YUY2(0)
            }
            y += sourceStride;
            u += halfstride;
            v += halfstride;
            out += dstStride[0U];
        }
    }
    else
    {
        for (line = 0; line < height; ++line)
        {
            uint32_t *idst = (uint32_t *)out;
            for (i = 0; i < (halfwidth - 7); i += 8)
            {
                YUV422_PACK_UYVY(0)
                YUV422_PACK_UYVY(1)
                YUV422_PACK_UYVY(2)
                YUV422_PACK_UYVY(3)
                YUV422_PACK_UYVY(4)
                YUV422_PACK_UYVY(5)
                YUV422_PACK_UYVY(6)
                YUV422_PACK_UYVY(7)
            }
            for (; i < halfwidth; ++i)
            {
                YUV422_PACK_UYVY(0)
            }
            y += sourceStride;
            u += halfstride;
            v += halfstride;
            out += dstStride[0U];
        }
    }

    av_freep(&pTmpBuffer);

    return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToAYUV(const uint8_t *const src[4U], const ptrdiff_t srcStride[4U], uint8_t *dst[],
                                           int width, int height, const ptrdiff_t dstStride[])
{
    const BYTE *y = nullptr;
    const BYTE *u = nullptr;
    const BYTE *v = nullptr;
    ptrdiff_t line, i = 0;
    ptrdiff_t sourceStride = 0;
    BYTE *pTmpBuffer = nullptr;

    if (m_InputPixFmt != LAVPixFmt_YUV444)
    {
        uint8_t *tmp[4U] = {nullptr};
        ptrdiff_t tmpStride[4U] = {0};
        ptrdiff_t const scaleStride = FFALIGN(width, 32);

        pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 3);
        if (pTmpBuffer == nullptr) {
            return E_OUTOFMEMORY;
}

        tmp[0U] = pTmpBuffer;
        tmp[1U] = tmp[0U] + (height * scaleStride);
        tmp[2U] = tmp[1U] + (height * scaleStride);
        tmp[3U] = nullptr;
        tmpStride[0U] = scaleStride;
        tmpStride[1U] = scaleStride;
        tmpStride[2U] = scaleStride;
        tmpStride[3U] = 0;

        SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), AV_PIX_FMT_YUV444P, SWS_BILINEAR);
        sws_scale2(ctx, src, srcStride, 0, height, tmp, tmpStride);

        y = tmp[0U];
        u = tmp[1U];
        v = tmp[2U];
        sourceStride = scaleStride;
    }
    else
    {
        y = src[0U];
        u = src[1U];
        v = src[2U];
        sourceStride = srcStride[0U];
    }

#define YUV444_PACK_AYUV(offset) *idst++ = v[i + offset] | (u[i + offset] << 8) | (y[i + offset] << 16) | (0xff << 24);

    BYTE *out = dst[0U];
    for (line = 0; line < height; ++line)
    {
        uint32_t *idst = (uint32_t *)out;
        for (i = 0; i < (width - 7); i += 8)
        {
            YUV444_PACK_AYUV(0)
            YUV444_PACK_AYUV(1)
            YUV444_PACK_AYUV(2)
            YUV444_PACK_AYUV(3)
            YUV444_PACK_AYUV(4)
            YUV444_PACK_AYUV(5)
            YUV444_PACK_AYUV(6)
            YUV444_PACK_AYUV(7)
        }
        for (; i < width; ++i)
        {
            YUV444_PACK_AYUV(0)
        }
        y += sourceStride;
        u += sourceStride;
        v += sourceStride;
        out += dstStride[0U];
    }

    av_freep(&pTmpBuffer);

    return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToPX1X(const uint8_t *const src[4U], const ptrdiff_t srcStride[4U], uint8_t *dst[],
                                           int width, int height, const ptrdiff_t dstStride[], int chromaVertical)
{
    const BYTE *y = nullptr;
    const BYTE *u = nullptr;
    const BYTE *v = nullptr;
    ptrdiff_t line, i = 0;
    ptrdiff_t sourceStride = 0;

    int shift = 0;

    BYTE *pTmpBuffer = nullptr;

    if ((chromaVertical == 1 && m_InputPixFmt != LAVPixFmt_YUV422bX) ||
        (chromaVertical == 2 && m_InputPixFmt != LAVPixFmt_YUV420bX))
    {
        uint8_t *tmp[4U] = {nullptr};
        ptrdiff_t tmpStride[4U] = {0};
        ptrdiff_t const scaleStride = FFALIGN(width, 32) * 2;

        pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 2);
        if (pTmpBuffer == nullptr) {
            return E_OUTOFMEMORY;
}

        tmp[0U] = pTmpBuffer;
        tmp[1U] = tmp[0U] + (height * scaleStride);
        tmp[2U] = tmp[1U] + ((height / chromaVertical) * (scaleStride / 2));
        tmp[3U] = nullptr;
        tmpStride[0U] = scaleStride;
        tmpStride[1U] = scaleStride / 2;
        tmpStride[2U] = scaleStride / 2;
        tmpStride[3U] = 0;

        SwsContext *ctx =
            GetSWSContext(width, height, GetFFInput(),
                          chromaVertical == 1 ? AV_PIX_FMT_YUV422P16LE : AV_PIX_FMT_YUV420P16LE, SWS_BILINEAR);
        sws_scale2(ctx, src, srcStride, 0, height, tmp, tmpStride);

        y = tmp[0U];
        u = tmp[1U];
        v = tmp[2U];
        sourceStride = scaleStride;
    }
    else
    {
        y = src[0U];
        u = src[1U];
        v = src[2U];
        sourceStride = srcStride[0U];

        shift = (16 - m_InBpp);
    }

    // copy Y
    BYTE *pLineOut = dst[0U];
    const BYTE *pLineIn = y;
    for (line = 0; line < height; ++line)
    {
        if (shift == 0)
        {
            memcpy(pLineOut, pLineIn, width * 2);
        }
        else
        {
            const uint16_t *yc = (uint16_t *)pLineIn;
            uint16_t *idst = (uint16_t *)pLineOut;
            for (i = 0; i < width; ++i)
            {
                uint16_t yv = AV_RL16(yc + i);
                if (shift) {
                    yv <<= shift;
}
                *idst++ = yv;
            }
        }
        pLineOut += dstStride[0U];
        pLineIn += sourceStride;
    }

    sourceStride >>= 2;

    // Merge U/V
    BYTE *out = dst[1U];
    const uint16_t *uc = (uint16_t *)u;
    const uint16_t *vc = (uint16_t *)v;
    for (line = 0; line < height / chromaVertical; ++line)
    {
        uint32_t *idst = (uint32_t *)out;
        for (i = 0; i < width / 2; ++i)
        {
            uint16_t uv = AV_RL16(uc + i);
            uint16_t vv = AV_RL16(vc + i);
            if (shift)
            {
                uv <<= shift;
                vv <<= shift;
            }
            *idst++ = uv | (vv << 16);
        }
        uc += sourceStride;
        vc += sourceStride;
        out += dstStride[1U];
    }

    av_freep(&pTmpBuffer);

    return S_OK;
}

#define YUV444_PACKED_LOOP_HEAD(width, height, y, u, v, out) \
    for (int line = 0; line < height; ++line)                \
    {                                                        \
        uint32_t *idst = (uint32_t *)out;                    \
        for (int i = 0; i < width; ++i)                      \
        {                                                    \
            uint32_t yv, uv, vv;

#define YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out) \
    YUV444_PACKED_LOOP_HEAD(width, height, y, u, v, out)        \
    yv = AV_RL16(y + i);                                        \
    uv = AV_RL16(u + i);                                        \
    vv = AV_RL16(v + i);

#define YUV444_PACKED_LOOP_END(y, u, v, out, srcStride, dstStride) \
    }                                                              \
    y += srcStride;                                                \
    u += srcStride;                                                \
    v += srcStride;                                                \
    out += dstStride;                                              \
    }

HRESULT CLAVPixFmtConverter::ConvertToY410(const uint8_t *const src[4U], const ptrdiff_t srcStride[4U], uint8_t *dst[],
                                           int width, int height, const ptrdiff_t dstStride[])
{
    const uint16_t *y = nullptr;
    const uint16_t *u = nullptr;
    const uint16_t *v = nullptr;
    ptrdiff_t sourceStride = 0;
    bool b9Bit = false;

    BYTE *pTmpBuffer = nullptr;

    if (m_InputPixFmt != LAVPixFmt_YUV444bX || m_InBpp > 10)
    {
        uint8_t *tmp[4U] = {nullptr};
        ptrdiff_t tmpStride[4U] = {0};
        ptrdiff_t const scaleStride = FFALIGN(width, 32);

        pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);
        if (pTmpBuffer == nullptr) {
            return E_OUTOFMEMORY;
}

        tmp[0U] = pTmpBuffer;
        tmp[1U] = tmp[0U] + (height * scaleStride * 2);
        tmp[2U] = tmp[1U] + (height * scaleStride * 2);
        tmp[3U] = nullptr;
        tmpStride[0U] = scaleStride * 2;
        tmpStride[1U] = scaleStride * 2;
        tmpStride[2U] = scaleStride * 2;
        tmpStride[3U] = 0;

        SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), AV_PIX_FMT_YUV444P10LE, SWS_BILINEAR);
        sws_scale2(ctx, src, srcStride, 0, height, tmp, tmpStride);

        y = (uint16_t *)tmp[0U];
        u = (uint16_t *)tmp[1U];
        v = (uint16_t *)tmp[2U];
        sourceStride = scaleStride;
    }
    else
    {
        y = (uint16_t *)src[0U];
        u = (uint16_t *)src[1U];
        v = (uint16_t *)src[2U];
        sourceStride = srcStride[0U] / 2;

        b9Bit = (m_InBpp == 9);
    }

#define YUV444_Y410_PACK *idst++ = (uv & 0x3FF) | ((yv & 0x3FF) << 10) | ((vv & 0x3FF) << 20) | (3 << 30);

    BYTE *out = dst[0U];
    YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out)
    if (b9Bit)
    {
        yv <<= 1;
        uv <<= 1;
        vv <<= 1;
    }
    YUV444_Y410_PACK
    YUV444_PACKED_LOOP_END(y, u, v, out, sourceStride, dstStride[0U])

    av_freep(&pTmpBuffer);

    return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertToY416(const uint8_t *const src[4U], const ptrdiff_t srcStride[4U], uint8_t *dst[],
                                           int width, int height, const ptrdiff_t dstStride[])
{
    const uint16_t *y = nullptr;
    const uint16_t *u = nullptr;
    const uint16_t *v = nullptr;
    ptrdiff_t sourceStride = 0;

    BYTE *pTmpBuffer = nullptr;

    int shift = (16 - m_InBpp);
    if (m_InputPixFmt != LAVPixFmt_YUV444bX)
    {
        uint8_t *tmp[4U] = {nullptr};
        ptrdiff_t tmpStride[4U] = {0};
        ptrdiff_t const scaleStride = FFALIGN(width, 32);

        pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);
        if (pTmpBuffer == nullptr) {
            return E_OUTOFMEMORY;
}

        tmp[0U] = pTmpBuffer;
        tmp[1U] = tmp[0U] + (height * scaleStride * 2);
        tmp[2U] = tmp[1U] + (height * scaleStride * 2);
        tmp[3U] = nullptr;
        tmpStride[0U] = scaleStride * 2;
        tmpStride[1U] = scaleStride * 2;
        tmpStride[2U] = scaleStride * 2;
        tmpStride[3U] = 0;

        SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), AV_PIX_FMT_YUV444P16LE, SWS_BILINEAR);
        sws_scale2(ctx, src, srcStride, 0, height, tmp, tmpStride);

        y = (uint16_t *)tmp[0U];
        u = (uint16_t *)tmp[1U];
        v = (uint16_t *)tmp[2U];
        sourceStride = scaleStride;
        shift = 0;
    }
    else
    {
        y = (uint16_t *)src[0U];
        u = (uint16_t *)src[1U];
        v = (uint16_t *)src[2U];
        sourceStride = srcStride[0U] / 2;
    }

    BYTE *out = dst[0U];
    YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out)
    uint16_t *p = (uint16_t *)idst;
    p[0U] = (uv << shift);
    p[1U] = (yv << shift);
    p[2U] = (vv << shift);
    p[3U] = 0xFFFF;

    idst += 2;
    YUV444_PACKED_LOOP_END(y, u, v, out, sourceStride, dstStride[0U])

    av_freep(&pTmpBuffer);

    return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertTov210(const uint8_t *const src[4U], const ptrdiff_t srcStride[4U], uint8_t *dst[],
                                           int width, int height, const ptrdiff_t dstStride[])
{
    const uint16_t *y = nullptr;
    const uint16_t *u = nullptr;
    const uint16_t *v = nullptr;
    ptrdiff_t srcyStride = 0;
    ptrdiff_t srcuvStride = 0;

    BYTE *pTmpBuffer = nullptr;

    if (m_InputPixFmt != LAVPixFmt_YUV422bX || m_InBpp != 10)
    {
        uint8_t *tmp[4U] = {nullptr};
        ptrdiff_t tmpStride[4U] = {0};
        ptrdiff_t const scaleStride = FFALIGN(width, 32);

        pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);
        if (pTmpBuffer == nullptr) {
            return E_OUTOFMEMORY;
}

        tmp[0U] = pTmpBuffer;
        tmp[1U] = tmp[0U] + (height * scaleStride * 2);
        tmp[2U] = tmp[1U] + (height * scaleStride * 2);
        tmp[3U] = nullptr;
        tmpStride[0U] = scaleStride * 2;
        tmpStride[1U] = scaleStride;
        tmpStride[2U] = scaleStride;
        tmpStride[3U] = 0;

        SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), AV_PIX_FMT_YUV422P10LE, SWS_BILINEAR);
        sws_scale2(ctx, src, srcStride, 0, height, tmp, tmpStride);

        y = (uint16_t *)tmp[0U];
        u = (uint16_t *)tmp[1U];
        v = (uint16_t *)tmp[2U];
        srcyStride = scaleStride;
        srcuvStride = scaleStride >> 1;
    }
    else
    {
        y = (uint16_t *)src[0U];
        u = (uint16_t *)src[1U];
        v = (uint16_t *)src[2U];
        srcyStride = srcStride[0U] >> 1;
        srcuvStride = srcStride[1U] >> 1;
    }

    // Calculate v210 stride
    ptrdiff_t const outStride = (((dstStride[0U] >> 2) + 47) / 48) * 128;

    // Align width to an even number for processing
    // This may read into the source stride, but otherwise the algorithm won't work.
    width = FFALIGN(width, 2);

    BYTE *pdst = dst[0U];
    uint32_t *p = (uint32_t *)pdst;
    int w;

//#define CLIP(v) av_clip(v, 4, 1019)
#define CLIP(v) (v & 0x03FF)
#define WRITE_PIXELS(a, b, c)                           \
    do                                                  \
    {                                                   \
        val = CLIP(*a++);                               \
        val |= (CLIP(*b++) << 10) | (CLIP(*c++) << 20); \
        *p++ = val;                                     \
    } while (0)

    for (int h = 0; h < height; ++h)
    {
        uint32_t val;
        for (w = 0; w < width - 5; w += 6)
        {
            WRITE_PIXELS(u, y, v);
            WRITE_PIXELS(y, u, y);
            WRITE_PIXELS(v, y, u);
            WRITE_PIXELS(y, v, y);
        }
        if (w < width - 1)
        {
            WRITE_PIXELS(u, y, v);

            val = CLIP(*y++);
            if (w == width - 2) {
                *p++ = val;
}
            if (w < width - 3)
            {
                val |= (CLIP(*u++) << 10) | (CLIP(*y++) << 20);
                *p++ = val;

                val = CLIP(*v++) | (CLIP(*y++) << 10);
                *p++ = val;
            }
        }

        pdst += outStride;
        memset(p, 0, pdst - (BYTE *)p);
        p = (uint32_t *)pdst;
        y += srcyStride - width;
        u += srcuvStride - (width >> 1);
        v += srcuvStride - (width >> 1);
    }
    av_freep(&pTmpBuffer);

    return S_OK;
}

HRESULT CLAVPixFmtConverter::ConvertTov410(const uint8_t *const src[4U], const ptrdiff_t srcStride[4U], uint8_t *dst[],
                                           int width, int height, const ptrdiff_t dstStride[])
{
    const uint16_t *y = nullptr;
    const uint16_t *u = nullptr;
    const uint16_t *v = nullptr;
    ptrdiff_t sourceStride = 0;
    bool b9Bit = false;

    BYTE *pTmpBuffer = nullptr;

    if (m_InputPixFmt != LAVPixFmt_YUV444bX || m_InBpp > 10)
    {
        uint8_t *tmp[4U] = {nullptr};
        ptrdiff_t tmpStride[4U] = {0};
        ptrdiff_t const scaleStride = FFALIGN(width, 32);

        pTmpBuffer = (BYTE *)av_malloc(height * scaleStride * 6);
        if (pTmpBuffer == nullptr) {
            return E_OUTOFMEMORY;
}

        tmp[0U] = pTmpBuffer;
        tmp[1U] = tmp[0U] + (height * scaleStride * 2);
        tmp[2U] = tmp[1U] + (height * scaleStride * 2);
        tmp[3U] = nullptr;
        tmpStride[0U] = scaleStride * 2;
        tmpStride[1U] = scaleStride * 2;
        tmpStride[2U] = scaleStride * 2;
        tmpStride[3U] = 0;

        SwsContext *ctx = GetSWSContext(width, height, GetFFInput(), AV_PIX_FMT_YUV444P10LE, SWS_BILINEAR);
        sws_scale2(ctx, src, srcStride, 0, height, tmp, tmpStride);

        y = (uint16_t *)tmp[0U];
        u = (uint16_t *)tmp[1U];
        v = (uint16_t *)tmp[2U];
        sourceStride = scaleStride;
    }
    else
    {
        y = (uint16_t *)src[0U];
        u = (uint16_t *)src[1U];
        v = (uint16_t *)src[2U];
        sourceStride = srcStride[0U] / 2;

        b9Bit = (m_InBpp == 9);
    }

#define YUV444_v410_PACK *idst++ = ((uv & 0x3FF) << 2) | ((yv & 0x3FF) << 12) | ((vv & 0x3FF) << 22);

    BYTE *out = dst[0U];
    YUV444_PACKED_LOOP_HEAD_LE(width, height, y, u, v, out)
    if (b9Bit)
    {
        yv <<= 1;
        uv <<= 1;
        vv <<= 1;
    }
    YUV444_v410_PACK YUV444_PACKED_LOOP_END(y, u, v, out, sourceStride, dstStride[0U])

        av_freep(&pTmpBuffer);

    return S_OK;
}
