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

#include "stdafx.h"

#include "FontInstaller.h"

#include <cstdint>


CFontInstaller::CFontInstaller()
    : pAddFontMemResourceEx{nullptr}
    , pRemoveFontMemResourceEx{nullptr}
{
    if (HMODULE hGdi = GetModuleHandle(_T("gdi32.dll")))
    {
        pAddFontMemResourceEx =
            (HANDLE(WINAPI *)(PVOID, DWORD, PVOID, DWORD *))GetProcAddress(hGdi, "AddFontMemResourceEx");
        pRemoveFontMemResourceEx = (BOOL(WINAPI *)(HANDLE))GetProcAddress(hGdi, "RemoveFontMemResourceEx");
    }
}

CFontInstaller::~CFontInstaller()
{
    UninstallFonts();
}

bool CFontInstaller::InstallFont(const void *pData, UINT len)
{
    return InstallFontMemory(pData, len);
}

void CFontInstaller::UninstallFonts()
{
    if (pRemoveFontMemResourceEx)
    {
        std::vector<HANDLE>::iterator it;
        for (it = m_fonts.begin(); it != m_fonts.end(); ++it)
        {
            pRemoveFontMemResourceEx(*it);
        }
        m_fonts.clear();
    }
}

static bool IsPlausibleSfntFont(const void *pData, UINT len)
{
    // Reject a truncated or oversized table directory before GDI parses the font.
    auto const *data = static_cast<const BYTE *>(pData);
    if ((data == nullptr) || (len < 12) || (len > 16U * 1024U * 1024U))
    {
        return false;
    }

    std::uint32_t const scaler = (static_cast<std::uint32_t>(data[0]) << 24) | (static_cast<std::uint32_t>(data[1]) << 16) |
                            (static_cast<std::uint32_t>(data[2]) << 8) | data[3];
    // 0x00010000, 'true', 'OTTO', 'typ1'
    if ((scaler != 0x00010000U) && (scaler != 0x74727565U) && (scaler != 0x4F54544FU) && (scaler != 0x74797031U))
    {
        return false;
    }

    std::uint16_t const numTables = static_cast<std::uint16_t>((data[4] << 8) | data[5]);
    if ((numTables == 0) || (numTables > 64))
    {
        return false;
    }
    std::uint64_t const directoryBytes = 12ull + static_cast<std::uint64_t>(numTables) * 16ull;
    if (directoryBytes > len)
    {
        return false;
    }

    for (std::uint16_t i{ 0U }; i < numTables; ++i)
    {
        auto const *rec = data + 12 + static_cast<UINT>(i) * 16U;
        std::uint32_t const offset = (static_cast<std::uint32_t>(rec[8]) << 24) | (static_cast<std::uint32_t>(rec[9]) << 16) |
                                (static_cast<std::uint32_t>(rec[10]) << 8) | rec[11];
        std::uint32_t const length = (static_cast<std::uint32_t>(rec[12]) << 24) | (static_cast<std::uint32_t>(rec[13]) << 16) |
                                (static_cast<std::uint32_t>(rec[14]) << 8) | rec[15];
        if ((length > len) || (offset > len - length))
        {
            return false;
        }
    }
    return true;
}

bool CFontInstaller::InstallFontMemory(const void *pData, UINT len)
{
    if (!pAddFontMemResourceEx || !IsPlausibleSfntFont(pData, len))
    {
        return false;
    }

    DWORD nFonts{ 0U };
    HANDLE hFont = pAddFontMemResourceEx((PVOID)pData, len, nullptr, &nFonts);
    if (hFont && (nFonts > 0))
    {
        m_fonts.push_back(hFont);
    }
    return hFont && nFonts > 0;
}
