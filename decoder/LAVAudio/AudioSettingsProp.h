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

#include "BaseDSPropPage.h"
#include "LAVAudioSettings.h"
#include "Media.h"

#include <array>


// {2D8F1801-A70D-48F4-B76B-7F5AE022AB54}
DEFINE_GUID(CLSID_LAVAudioSettingsProp, 0x2d8f1801, 0xa70d, 0x48f4, 0xb7, 0x6b, 0x7f, 0x5a, 0xe0, 0x22, 0xab, 0x54);

// {C89FC33C-E60A-4C97-BEF4-ACC5762B6404}
DEFINE_GUID(CLSID_LAVAudioMixingProp, 0xc89fc33c, 0xe60a, 0x4c97, 0xbe, 0xf4, 0xac, 0xc5, 0x76, 0x2b, 0x64, 0x4);

// {20ED4A03-6AFD-4FD9-980B-2F6143AA0892}
DEFINE_GUID(CLSID_LAVAudioStatusProp, 0x20ed4a03, 0x6afd, 0x4fd9, 0x98, 0xb, 0x2f, 0x61, 0x43, 0xaa, 0x8, 0x92);

// {BD72668E-6BFF-4CD1-8480-D465708B336B}
DEFINE_GUID(CLSID_LAVAudioFormatsProp, 0xbd72668e, 0x6bff, 0x4cd1, 0x84, 0x80, 0xd4, 0x65, 0x70, 0x8b, 0x33, 0x6b);

class CLAVAudioSettingsProp : public CBaseDSPropPage
{
  public:
    CLAVAudioSettingsProp(LPUNKNOWN pUnk, HRESULT *phr);
    ~CLAVAudioSettingsProp();

    HRESULT OnActivate();
    HRESULT OnConnect(IUnknown *pUnk);
    HRESULT OnDisconnect();
    HRESULT OnApplyChanges();
    INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    HRESULT LoadData();

    void SetDirty()
    {
        m_bDirty = TRUE;
        if (m_pPageSite)
        {
            m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
        }
    }

  private:
    ILAVAudioSettings *m_pAudioSettings{ nullptr };

    BOOL m_bDRCEnabled{ FALSE };
    int m_iDRCLevel{ 0 };

    std::array<bool, Bitstream_NB> m_bBitstreaming{};
    BOOL m_bDTSHDFraming{ FALSE };
    BOOL m_bBitstreamingFallback{ FALSE };
    BOOL m_bAutoAVSync{ FALSE };
    BOOL m_bOutputStdLayout{ FALSE };
    BOOL m_bOutput51Legacy{ FALSE };
    BOOL m_bExpandMono{ FALSE };
    BOOL m_bExpand61{ FALSE };
    std::array<bool, SampleFormat_NB> m_bSampleFormats{};
    BOOL m_bDither{ FALSE };
    BOOL m_bAudioDelay{ FALSE };
    int m_iAudioDelay{ 0 };
    BOOL m_TrayIcon{ FALSE };
};

class CLAVAudioMixingProp : public CBaseDSPropPage
{
  public:
    CLAVAudioMixingProp(LPUNKNOWN pUnk, HRESULT *phr);
    ~CLAVAudioMixingProp();

    HRESULT OnActivate();
    HRESULT OnConnect(IUnknown *pUnk);
    HRESULT OnDisconnect();
    HRESULT OnApplyChanges();
    INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    HRESULT LoadData();

    void SetDirty()
    {
        m_bDirty = TRUE;
        if (m_pPageSite)
        {
            m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
        }
    }

  private:
    ILAVAudioSettings *m_pAudioSettings{ nullptr };

    BOOL m_bMixing{ FALSE };
    DWORD m_dwSpeakerLayout{ 0U };
    DWORD m_dwFlags{ 0U };
    DWORD m_dwMixingMode{ 0U };
    DWORD m_dwMixCenter{ 0U };
    DWORD m_dwMixSurround{ 0U };
    DWORD m_dwMixLFE{ 0U };
};

class CLAVAudioFormatsProp : public CBaseDSPropPage
{
  public:
    CLAVAudioFormatsProp(LPUNKNOWN pUnk, HRESULT *phr);
    ~CLAVAudioFormatsProp();

    HRESULT OnActivate();
    HRESULT OnConnect(IUnknown *pUnk);
    HRESULT OnDisconnect();
    HRESULT OnApplyChanges();
    INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    HRESULT LoadData();

    void SetDirty()
    {
        m_bDirty = TRUE;
        if (m_pPageSite)
        {
            m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
        }
    }

  private:
    ILAVAudioSettings *m_pAudioSettings{ nullptr };

    std::array<bool, Codec_AudioNB> m_bFormats{};
};

class CLAVAudioStatusProp : public CBaseDSPropPage
{
  public:
    CLAVAudioStatusProp(LPUNKNOWN pUnk, HRESULT *phr);
    ~CLAVAudioStatusProp();

    HRESULT OnActivate();
    HRESULT OnDeactivate();
    HRESULT OnConnect(IUnknown *pUnk);
    HRESULT OnDisconnect();
    INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    void UpdateVolumeDisplay();

  private:
    ILAVAudioStatus *m_pAudioStatus{ nullptr };
    int m_nChannels{ 0 };
};
