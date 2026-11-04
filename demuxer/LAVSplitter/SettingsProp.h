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
#include "LAVSplitterSettingsInternal.h"

#include <array>


// GUID: a19de2f2-2f74-4927-8436-61129d26c141
DEFINE_GUID(CLSID_LAVSplitterSettingsProp, 0xa19de2f2, 0x2f74, 0x4927, 0x84, 0x36, 0x61, 0x12, 0x9d, 0x26, 0xc1, 0x41);

// {56904B22-091C-4459-A2E6-B1F4F946B55F}
DEFINE_GUID(CLSID_LAVSplitterFormatsProp, 0x56904b22, 0x91c, 0x4459, 0xa2, 0xe6, 0xb1, 0xf4, 0xf9, 0x46, 0xb5, 0x5f);

#define LANG_BUFFER_SIZE 16384

class CLAVSplitterSettingsProp : public CBaseDSPropPage
{
  public:
    CLAVSplitterSettingsProp(LPUNKNOWN pUnk, HRESULT *phr);
    virtual ~CLAVSplitterSettingsProp(void);

    HRESULT OnActivate();
    HRESULT OnConnect(IUnknown *pUnk);
    HRESULT OnDisconnect();
    HRESULT OnApplyChanges();
    INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    HRESULT LoadData();
    void UpdateSubtitleMode(LAVSubtitleMode mode);

    void SetDirty()
    {
        m_bDirty = TRUE;
        if (m_pPageSite)
        {
            m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
        }
    }

  private:
    ILAVFSettingsInternal *m_pLAVF{ nullptr };

    // Settings
    WCHAR *m_pszPrefLang{ nullptr };
    WCHAR *m_pszPrefSubLang{ nullptr };
    WCHAR *m_pszAdvSubConfig{ nullptr };

    LAVSubtitleMode m_subtitleMode;
    BOOL m_PGSForcedStream{ FALSE };
    BOOL m_PGSOnlyForced{ FALSE };
    int m_VC1Mode{ 0 };
    BOOL m_substreams{ FALSE };
    BOOL m_MKVExternal{ FALSE };

    BOOL m_StreamSwitchReselectSubs{ FALSE };

    BOOL m_StreamSwitchRemoveAudio{ FALSE };
    BOOL m_PreferHighQualityAudio{ FALSE };
    BOOL m_ImpairedAudio{ FALSE };
    DWORD m_QueueMaxMem{ 0U };
    DWORD m_QueueMaxPackets{ 0U };
    DWORD m_NetworkAnalysisDuration{ 0U };

    BOOL m_TrayIcon{ FALSE };

    LAVSubtitleMode m_selectedSubMode;
    std::array<WCHAR, LANG_BUFFER_SIZE> m_subLangBuffer{};
    std::array<WCHAR, LANG_BUFFER_SIZE> m_advSubBuffer{};

    std::array<WCHAR, 256U> stringBuffer{};
};

class CLAVSplitterFormatsProp : public CBaseDSPropPage
{
  public:
    CLAVSplitterFormatsProp(LPUNKNOWN pUnk, HRESULT *phr);
    virtual ~CLAVSplitterFormatsProp(void);

    HRESULT OnActivate();
    HRESULT OnConnect(IUnknown *pUnk);
    HRESULT OnDisconnect();
    HRESULT OnApplyChanges();
    INT_PTR OnReceiveMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

  private:
    void SetDirty()
    {
        m_bDirty = TRUE;
        if (m_pPageSite)
        {
            m_pPageSite->OnStatusChange(PROPPAGESTATUS_DIRTY);
        }
    }

  private:
    ILAVFSettingsInternal *m_pLAVF{ nullptr };

    std::set<FormatInfo> m_Formats;
    BOOL *m_bFormats{ nullptr };

    std::array<WCHAR, 256U> stringBuffer{};
};
