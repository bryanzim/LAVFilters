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

#include "../LAVSubtitleConsumer.h"

#include <cstddef>
#include <cstdint>


#define FAST_DIV255(x) ((((x) + 128) * 257) >> 16)

DECLARE_BLEND_FUNC_IMPL(blend_rgb_c)
{
    ASSERT((pixFmt == LAVPixFmt_RGB32 || pixFmt == LAVPixFmt_RGB24));

    BYTE *rgbOut = video[0U];
    const BYTE *subIn = subData[0U];

    const std::ptrdiff_t outStride = videoStride[0U];
    const std::ptrdiff_t inStride = subStride[0U];

    const std::ptrdiff_t dstep = (pixFmt == LAVPixFmt_RGB24) ? 3 : 4;

    for (int y{ 0 }; y < size.cy; ++y)
    {
        BYTE *dstLine = rgbOut + ((y + position.y) * outStride) + (position.x * dstep);
        const BYTE *srcLine = subIn + (y * inStride);
        for (int x{ 0 }; x < size.cx; ++x)
        {
            const BYTE a = srcLine[3U];
            switch (a)
            {
            case 0: break;
            case 255:
                dstLine[0U] = srcLine[0U];
                dstLine[1U] = srcLine[1U];
                dstLine[2U] = srcLine[2U];
                break;
            default:
                dstLine[0U] = av_clip_uint8(FAST_DIV255(dstLine[0U] * (255 - a)) + srcLine[0U]);
                dstLine[1U] = av_clip_uint8(FAST_DIV255(dstLine[1U] * (255 - a)) + srcLine[1U]);
                dstLine[2U] = av_clip_uint8(FAST_DIV255(dstLine[2U] * (255 - a)) + srcLine[2U]);
                break;
            }
            dstLine += dstep;
            srcLine += 4;
        }
    }

    return S_OK;
}

template <class pixT, int nv12> DECLARE_BLEND_FUNC_IMPL(blend_yuv_c)
{
    ASSERT((pixFmt == LAVPixFmt_YUV420 || pixFmt == LAVPixFmt_NV12 || pixFmt == LAVPixFmt_YUV422 ||
           pixFmt == LAVPixFmt_YUV444 || pixFmt == LAVPixFmt_YUV420bX || pixFmt == LAVPixFmt_YUV422bX ||
           pixFmt == LAVPixFmt_YUV444bX || pixFmt == LAVPixFmt_P016));

    BYTE *y = video[0U];
    BYTE *u = video[1U];
    BYTE *v = video[2U];

    const BYTE *subY = subData[0U];
    const BYTE *subU = subData[1U];
    const BYTE *subV = subData[2U];
    const BYTE *subA = subData[3U];

    const std::ptrdiff_t outStride = videoStride[0U];
    const std::ptrdiff_t outStrideUV = videoStride[1U];
    const std::ptrdiff_t inStride = subStride[0U];
    const std::ptrdiff_t inStrideUV = subStride[1U];

    int line, col;
    int w = size.cx, h = size.cy;
    int yPos = position.y;
    int xPos = position.x;

    const int hsub = nv12 || (pixFmt == LAVPixFmt_YUV420 || pixFmt == LAVPixFmt_YUV420bX || pixFmt == LAVPixFmt_NV12);
    const int vsub = nv12 || (pixFmt != LAVPixFmt_YUV444 && pixFmt != LAVPixFmt_YUV444bX);
    const int shift = sizeof(pixT) > 1 ? bpp - 8 : 0;

    for (line = 0; line < h; ++line)
    {
        pixT *dstY = reinterpret_cast<pixT *>((y + ((line + yPos) * outStride))) + xPos;
        const BYTE *srcY = subY + (line * inStride);
        const BYTE *srcA = subA + (line * inStride);
        for (col = 0; col < w; ++col)
        {
            switch (srcA[col])
            {
            case 0: break;
            case 255: dstY[col] = srcY[col] << shift; break;
            default: dstY[col] = FAST_DIV255(dstY[col] * (255 - srcA[col]) + (srcY[col] << shift) * srcA[col]); break;
            }
        }
    }

    if (hsub)
    {
        w >>= 1;
        xPos >>= 1;
    }
    if (vsub)
    {
        h >>= 1;
        yPos >>= 1;
    }

    for (line = 0; line < h; ++line)
    {
        pixT *dstUV = reinterpret_cast<pixT *>((u + (line + yPos) * outStrideUV)) + (xPos << 1);

        pixT *dstU = reinterpret_cast<pixT *>((u + (line + yPos) * outStrideUV)) + xPos;
        const BYTE *srcU = subU + line * inStrideUV;

        pixT *dstV = reinterpret_cast<pixT *>((v + (line + yPos) * outStrideUV)) + xPos;
        const BYTE *srcV = subV + line * inStrideUV;

        const BYTE *srcA = subA + (line * inStride * (std::ptrdiff_t(1) << hsub));
        for (col = 0; col < w; ++col)
        {
            // Average Alpha
            int alpha;
            if (hsub && vsub && (col + 1 < w) && (line + 1 < h))
            {
                alpha = (srcA[0U] + srcA[inStride] + srcA[1U] + srcA[inStride + 1]) >> 2;
            }
            else if (hsub || vsub)
            {
                int const alpha_h = hsub && col + 1 < w ? (srcA[0U] + srcA[1U]) >> 1 : srcA[0U];
                int const alpha_v = vsub && line + 1 < h ? (srcA[0U] + srcA[inStride]) >> 1 : srcA[0U];
                alpha = (alpha_h + alpha_v) >> 1;
            }
            else
            {
                alpha = srcA[0U];
            }
            if (nv12)
            {
                switch (alpha)
                {
                case 0: break;
                case 255:
                    dstUV[(col << 1) + 0] = srcU[col] << shift;
                    dstUV[(col << 1) + 1] = srcV[col] << shift;
                    break;
                default:
                    dstUV[(col << 1) + 0] =
                        FAST_DIV255(dstUV[(col << 1) + 0] * (255 - alpha) + (srcU[col] << shift) * alpha);
                    dstUV[(col << 1) + 1] =
                        FAST_DIV255(dstUV[(col << 1) + 1] * (255 - alpha) + (srcV[col] << shift) * alpha);
                    break;
                }
            }
            else
            {
                switch (alpha)
                {
                case 0: break;
                case 255:
                    dstU[col] = srcU[col] << shift;
                    dstV[col] = srcV[col] << shift;
                    break;
                default:
                    dstU[col] = FAST_DIV255(dstU[col] * (255 - alpha) + (srcU[col] << shift) * alpha);
                    dstV[col] = FAST_DIV255(dstV[col] * (255 - alpha) + (srcV[col] << shift) * alpha);
                    break;
                }
            }
            srcA += std::ptrdiff_t(1) << vsub;
        }
    }

    return S_OK;
}

template HRESULT CLAVSubtitleConsumer::blend_yuv_c<std::uint8_t, 1> BLEND_FUNC_PARAMS;
template HRESULT CLAVSubtitleConsumer::blend_yuv_c<std::uint8_t, 0> BLEND_FUNC_PARAMS;
template HRESULT CLAVSubtitleConsumer::blend_yuv_c<std::uint16_t, 0> BLEND_FUNC_PARAMS;
template HRESULT CLAVSubtitleConsumer::blend_yuv_c<std::uint16_t, 1> BLEND_FUNC_PARAMS;
