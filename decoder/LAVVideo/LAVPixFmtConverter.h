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

#include "decoders/ILAVDecoder.h"
#include "LAVVideoSettings.h"

#include <emmintrin.h>

#include <array>
#include <cstddef>
#include <cstdint>


#define CONV_FUNC_PARAMS                                                                                       \
    (const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], const std::ptrdiff_t dstStride[4U], \
     int width, int height, LAVPixelFormat inputFormat, int bpp, LAVOutPixFmts outputFormat)

#define DECLARE_CONV_FUNC(name) HRESULT name CONV_FUNC_PARAMS

#define DECLARE_CONV_FUNC_IMPL(name) DECLARE_CONV_FUNC(CLAVPixFmtConverter::name)

#if defined(__clang__)
#define LAV_TARGET(feature) __attribute__((target(feature)))
#else
#define LAV_TARGET(feature)
#endif

// Important, when adding new pixel formats, they need to be added in LAVPixFmtConverter.cpp as well to the format
// descriptors
typedef struct
{
    GUID subtype;
    int bpp;
    int codedbytes;
    int planes;
    std::array<int, 4U> planeHeight{};
    std::array<int, 4U> planeWidth{};
} LAVOutPixFmtDesc;

typedef struct _RGBCoeffs
{
    __m128i Ysub;
    __m128i CbCr_center;
    __m128i rgb_add;
    __m128i cy;
    __m128i cR_Cr;
    __m128i cG_Cb_cG_Cr;
    __m128i cB_Cb;
} RGBCoeffs;

typedef int(__stdcall *YUVRGBConversionFunc)(const std::uint8_t *srcY, const std::uint8_t *srcU, const std::uint8_t *srcV,
                                             std::uint8_t *dst, int width, int height, std::ptrdiff_t srcStrideY,
                                             std::ptrdiff_t srcStrideUV, std::ptrdiff_t dstStride, std::ptrdiff_t sliceYStart,
                                             std::ptrdiff_t sliceYEnd, const RGBCoeffs *coeffs, const std::uint16_t *dithers);

extern LAVOutPixFmtDesc lav_pixfmt_desc[];

class CLAVPixFmtConverter
{
  public:
    explicit CLAVPixFmtConverter();
    ~CLAVPixFmtConverter();

    void SetSettings(ILAVVideoSettings *pSettings) { m_pSettings = pSettings; }

    BOOL SetInputFmt(enum LAVPixelFormat pixfmt, int bpp)
    {
        ASSERT((pixfmt != LAVPixFmt_None && pixfmt != LAVPixFmt_D3D11 && pixfmt != LAVPixFmt_DXVA2));
        if ((m_InputPixFmt != pixfmt) || (m_InBpp != bpp))
        {
            m_InputPixFmt = pixfmt;
            m_InBpp = bpp;
            DestroySWScale();
            SelectConvertFunction();
            return TRUE;
        }
        return FALSE;
    }
    HRESULT SetOutputPixFmt(enum LAVOutPixFmts pix_fmt)
    {
        m_OutputPixFmt = pix_fmt;
        DestroySWScale();
        SelectConvertFunction();
        return S_OK;
    }

    LAVOutPixFmts GetOutputBySubtype(const GUID *guid);
    LAVOutPixFmts GetPreferredOutput();

    [[nodiscard]] constexpr LAVOutPixFmts GetOutputPixFmt() noexcept { return m_OutputPixFmt; }
    void SetColorProps(DXVA2_ExtendedFormat props, int RGBOutputRange)
    {
        if ((props.value != m_ColorProps.value) || (swsOutputRange != RGBOutputRange))
        {
            DestroySWScale();
            m_ColorProps = props;
            swsOutputRange = RGBOutputRange;
        }
    }

    int GetNumMediaTypes();
    void GetMediaType(CMediaType *mt, int index, LONG biWidth, LONG biHeight, DWORD dwAspectX, DWORD dwAspectY,
                      REFERENCE_TIME rtAvgTime, BOOL bInterlaced = TRUE, BOOL bVIH1 = FALSE);
    BOOL IsAllowedSubtype(const GUID *guid);

    HRESULT Convert(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst, int width, int height,
                    std::ptrdiff_t dstStride, int planeHeight);
    HRESULT ConvertDirect(LAVFrame *pFrame, std::uint8_t *dst, int width, int height, std::ptrdiff_t dstStride, int planeHeight);

    [[nodiscard]] constexpr BOOL IsRGBConverterActive() noexcept { return m_bRGBConverter; }
    BOOL IsDirectModeSupported(std::uintptr_t dst, std::ptrdiff_t stride);

    DWORD GetImageSize(int width, int height, LAVOutPixFmts pixFmt = LAVOutPixFmt_None);

  private:
    [[nodiscard]] AVPixelFormat GetFFInput() { return getFFPixelFormatFromLAV(m_InputPixFmt, m_InBpp); }

    int GetFilteredFormatCount();
    LAVOutPixFmts GetFilteredFormat(int index);

    void SelectConvertFunction();
    void SelectConvertFunctionDirect();

    // Helper functions for convert_generic
    HRESULT swscale_scale(enum AVPixelFormat srcPix, enum AVPixelFormat dstPix, const std::uint8_t *const src[4U],
                          const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width, int height,
                          const std::ptrdiff_t dstStride[4U], LAVOutPixFmtDesc pixFmtDesc, bool swapPlanes12 = false);
    HRESULT ConvertTo422Packed(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width,
                               int height, const std::ptrdiff_t dstStride[4U]);
    HRESULT ConvertToAYUV(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width,
                          int height, const std::ptrdiff_t dstStride[4U]);
    HRESULT ConvertToPX1X(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width,
                          int height, const std::ptrdiff_t dstStride[4U], int chromaVertical);
    HRESULT ConvertToY410(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width,
                          int height, const std::ptrdiff_t dstStride[4U]);
    HRESULT ConvertToY416(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width,
                          int height, const std::ptrdiff_t dstStride[4U]);
    HRESULT ConvertTov210(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width,
                          int height, const std::ptrdiff_t dstStride[4U]);
    HRESULT ConvertTov410(const std::uint8_t *const src[4U], const std::ptrdiff_t srcStride[4U], std::uint8_t *dst[4U], int width,
                          int height, const std::ptrdiff_t dstStride[4U]);

    void DestroySWScale()
    {
        if (m_pSwsContext) {
            sws_freeContext(m_pSwsContext);
}
        m_pSwsContext = nullptr;
        if (m_rgbCoeffs) {
            _aligned_free(m_rgbCoeffs);
}
        m_rgbCoeffs = nullptr;
        if (m_pRandomDithers) {
            _aligned_free(m_pRandomDithers);
}
        m_pRandomDithers = nullptr;
    };
    SwsContext *GetSWSContext(int width, int height, enum AVPixelFormat srcPix, enum AVPixelFormat dstPix, int flags);

    void ChangeStride(const std::uint8_t *src, std::ptrdiff_t srcStride, std::uint8_t *dst, std::ptrdiff_t dstStride, int width, int height,
                      int planeHeight, LAVOutPixFmts format);

    typedef HRESULT(CLAVPixFmtConverter::*ConverterFn) CONV_FUNC_PARAMS;

    // Conversion function pointer
    ConverterFn convert;
    ConverterFn convert_direct;

    // Pixel Implementations
    DECLARE_CONV_FUNC(convert_generic);
    DECLARE_CONV_FUNC(plane_copy);
    DECLARE_CONV_FUNC(plane_copy_sse2);
    DECLARE_CONV_FUNC(convert_yuv444_ayuv);
    DECLARE_CONV_FUNC(convert_yuv444_ayuv_dither_le);
    DECLARE_CONV_FUNC(convert_yuv444_y410);
    DECLARE_CONV_FUNC(convert_yuv420_px1x_le);
    DECLARE_CONV_FUNC(convert_yuv420_nv12);
    DECLARE_CONV_FUNC(convert_yuv_yv);
    DECLARE_CONV_FUNC(convert_nv12_yv12);
    DECLARE_CONV_FUNC(convert_p010_nv12_sse2);
    DECLARE_CONV_FUNC(convert_y210_p210_sse4);
    template <int uyvy> DECLARE_CONV_FUNC(convert_yuv420_yuy2);
    template <int uyvy> DECLARE_CONV_FUNC(convert_yuv422_yuy2_uyvy);
    template <int uyvy> DECLARE_CONV_FUNC(convert_yuv422_yuy2_uyvy_dither_le);
    template <int nv12> DECLARE_CONV_FUNC(convert_yuv_yv_nv12_dither_le);

    DECLARE_CONV_FUNC(convert_rgb48_rgb32_ssse3);
    template <int out32> DECLARE_CONV_FUNC(convert_rgb48_rgb);

    DECLARE_CONV_FUNC(plane_copy_direct_sse4);
    DECLARE_CONV_FUNC(plane_copy_direct_nv12_sse4);
    DECLARE_CONV_FUNC(convert_nv12_yv12_direct_sse4);
    DECLARE_CONV_FUNC(convert_p010_nv12_direct_sse4);
    DECLARE_CONV_FUNC(convert_y210_p210_direct_sse4);

    DECLARE_CONV_FUNC(convert_yuy2_yv16_sse2);
    DECLARE_CONV_FUNC(convert_yuy2_yv16_direct_sse4);

    DECLARE_CONV_FUNC(convert_yuv_rgb);
    const RGBCoeffs *getRGBCoeffs(int width, int height);
    void InitRGBConvDispatcher();

    const std::uint16_t *GetRandomDitherCoeffs(int height, int coeffs, int bits, int line);

  private:
    LAVPixelFormat m_InputPixFmt = LAVPixFmt_None;
    LAVOutPixFmts m_OutputPixFmt = LAVOutPixFmt_YV12;
    int m_InBpp{ 0 };

    BOOL m_bDirectMode{ false };

    int swsWidth{ 0 };
    int swsHeight{ 0 };
    int swsOutputRange{ 0 };

    DXVA2_ExtendedFormat m_ColorProps;

    std::ptrdiff_t m_RequiredAlignment{ 0 };

    SwsContext *m_pSwsContext = nullptr;

    std::size_t m_nAlignedBufferSize{ 0U };
    std::uint8_t *m_pAlignedBuffer = nullptr;

    int m_NumThreads{ 1 };

    ILAVVideoSettings *m_pSettings = nullptr;

    RGBCoeffs *m_rgbCoeffs = nullptr;
    BOOL m_bRGBConverter{ FALSE };
    BOOL m_bRGBConvInit{ FALSE };

    // [out32][dithermode][ycgco][format][shift]
    YUVRGBConversionFunc m_RGBConvFuncs[2U][2U][2U][LAVPixFmt_NB][9U];

    std::uint16_t *m_pRandomDithers = nullptr;
    int m_ditherWidth{ 0 };
    int m_ditherHeight{ 0 };
    int m_ditherBits{ 0 };
};
