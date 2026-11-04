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

#include <cstddef>

class CH264SequenceParser
{
  public:
    explicit CH264SequenceParser();
    virtual ~CH264SequenceParser(void);

    HRESULT ParseNALs(const BYTE *buffer, std::size_t buflen, int nal_size);

  public:
    struct
    {
        int valid{ 0 };

        int profile{ 0 };
        int level{ 0 };
        int chroma{ 0 };
        int luma_bitdepth{ 0 };
        int chroma_bitdepth{ 0 };
        int ref_frames{ 0 };
        int interlaced{ 0 };
        int ar_present{ 0 };

        int full_range{ 0 };
        int primaries{ 0 };
        int trc{ 0 };
        int colorspace{ 0 };
    } sps;

    struct
    {
        int valid{ 0 };
    } pps;

  private:
    HRESULT ParseSPS(const BYTE *buffer, std::size_t buflen);
};
