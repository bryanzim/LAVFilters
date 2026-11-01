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

#include "common_defines.h"
#include "streams.h"

#ifdef _WIN32
#include <atlbase.h>
#include <atlconv.h>
#endif


#pragma warning(push)
#pragma warning(disable : 4244)
extern "C"
{
#define __STDC_CONSTANT_MACROS
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/channel_layout.h"
#include "libavutil/display.h"
#include "libavutil/intreadwrite.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"

#include "libbluray/bdnav/clpi_parse.h"
#include "libbluray/bluray.h"
}
#pragma warning(pop)

#define AVFORMAT_INTERNAL_H

#include "DShowUtil.h"
#include "util/log_control.h"

#ifdef _WIN32
#include <MMReg.h>
#include <Shlwapi.h>
#endif
typedef struct AVCodecTag
{
    enum AVCodecID id;
    unsigned int tag;
} AVCodecTag;


// TODO: reference additional headers your program requires here
