# One compiler and one architecture. The matrix build invokes this four times.

set(LAV_ROOT "${CMAKE_SOURCE_DIR}")

if(NOT LAV_COMPILER_TAG)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(LAV_COMPILER_TAG "clang")
    else()
        set(LAV_COMPILER_TAG "msvc")
    endif()
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LAV_ARCH_NAME "x64")
    set(LAV_ARCH_BIN "bin_x64")
    set(LAV_MFX_ARCH "x64")
else()
    set(LAV_ARCH_NAME "Win32")
    set(LAV_ARCH_BIN "bin_Win32")
    set(LAV_MFX_ARCH "win32")
endif()

set(LAV_DIST_DIR "${LAV_ROOT}/dist/${LAV_COMPILER_TAG}/${LAV_ARCH_NAME}")
set(LAV_DIST_DIR_DEBUG "${LAV_DIST_DIR}d")

message(STATUS "LAV Filters ${LAV_COMPILER_TAG} ${LAV_ARCH_NAME} -> ${LAV_DIST_DIR}")

function(lav_set_output_dirs target)
    # A generator expression stops the Visual Studio generator from appending
    # a per-configuration directory (dist/msvc/x64/Release).
    set(stage "$<IF:$<CONFIG:Debug>,${LAV_DIST_DIR_DEBUG},${LAV_DIST_DIR}>")
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${stage}"
        LIBRARY_OUTPUT_DIRECTORY "${stage}"
        ARCHIVE_OUTPUT_DIRECTORY "${stage}/lib")
endfunction()

add_library(lav_platform INTERFACE)
target_compile_definitions(lav_platform INTERFACE
    WIN32
    UNICODE
    _UNICODE
    $<$<CONFIG:Debug>:_DEBUG>
    $<$<NOT:$<CONFIG:Debug>>:NDEBUG>)
target_compile_options(lav_platform INTERFACE
    /W3
    $<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:MSVC>>:/Gw>
    $<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:MSVC>>:/Zo>
    $<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:MSVC>>:/GL>)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # The DirectShow base classes rely on MSVC extensions. Keep those warnings
    # from drowning the Clang build; they are not errors.
    target_compile_options(lav_platform INTERFACE
        /clang:-Wno-microsoft-exception-spec
        /clang:-Wno-reorder-ctor
        /clang:-Wno-microsoft-extra-qualification
        /clang:-Wno-overloaded-virtual
        /clang:-Wno-nonportable-include-path
        /clang:-Wno-logical-op-parentheses
        /clang:-Wno-deprecated-declarations)
endif()

set(LAV_FFMPEG_BUILD_DIR "" CACHE PATH
    "Out-of-tree FFmpeg build directory that contains config.h and libavutil/avconfig.h")

add_library(lav_common INTERFACE)
target_link_libraries(lav_common INTERFACE lav_platform)
if(LAV_FFMPEG_BUILD_DIR)
    # Generated headers (config.h, avconfig.h) live in the build tree.
    target_include_directories(lav_common INTERFACE "${LAV_FFMPEG_BUILD_DIR}")
endif()
target_include_directories(lav_common INTERFACE
    "${LAV_ROOT}/include"
    "${LAV_ROOT}/common/includes"
    "${LAV_ROOT}/common/baseclasses"
    "${LAV_ROOT}/ffmpeg"
    "${LAV_ROOT}/libbluray/src"
    "${LAV_ROOT}/common/DSUtilLite")

function(lav_configure_static target)
    target_link_libraries(${target} PRIVATE lav_platform)
    target_compile_definitions(${target} PRIVATE _LIB)
    lav_set_output_dirs(${target})
endfunction()

function(lav_configure_shared target)
    target_link_libraries(${target} PRIVATE lav_platform)
    target_compile_definitions(${target} PRIVATE _WINDOWS _USRDLL)
    target_link_options(${target} PRIVATE
        "/SUBSYSTEM:WINDOWS"
        "$<$<AND:$<CONFIG:Release>,$<C_COMPILER_ID:MSVC>>:/LTCG>"
        "$<$<NOT:$<CONFIG:Debug>>:/OPT:REF>"
        "$<$<NOT:$<CONFIG:Debug>>:/OPT:ICF>"
        "$<$<NOT:$<CONFIG:Debug>>:/RELEASE>"
        "$<$<NOT:$<CONFIG:Debug>>:/DEBUG>"
        "$<$<NOT:$<CONFIG:Debug>>:/INCREMENTAL:NO>")
    lav_set_output_dirs(${target})
endfunction()

function(lav_enable_sse2 target)
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(${target} PRIVATE "$<$<CONFIG:Release>:/arch:SSE2>")
    endif()
endfunction()

# ---------------------------------------------------------------------------
# FFmpeg import libraries produced by build_ffmpeg.sh / build_ffmpeg_msvc.sh
# ---------------------------------------------------------------------------

set(LAV_FFMPEG_LIB_DIR "${LAV_ROOT}/${LAV_ARCH_BIN}/lib" CACHE PATH
    "Directory containing the FFmpeg import libraries (avcodec-lav.lib and friends)")
set(LAV_FFMPEG_BIN_DIR "${LAV_ROOT}/${LAV_ARCH_BIN}" CACHE PATH
    "Directory containing the FFmpeg runtime DLLs")

set(LAV_FFMPEG_HEADERS_READY FALSE)
if(LAV_FFMPEG_BUILD_DIR AND EXISTS "${LAV_FFMPEG_BUILD_DIR}/libavutil/avconfig.h")
    set(LAV_FFMPEG_HEADERS_READY TRUE)
elseif(EXISTS "${LAV_ROOT}/ffmpeg/libavutil/avconfig.h")
    set(LAV_FFMPEG_HEADERS_READY TRUE)
endif()

set(LAV_FFMPEG_FOUND TRUE)
foreach(lib_name avutil avcodec avformat swscale swresample avfilter)
    find_library(LAV_LIB_${lib_name}
        NAMES "${lib_name}-lav"
        PATHS "${LAV_FFMPEG_LIB_DIR}"
        NO_DEFAULT_PATH)
    if(LAV_LIB_${lib_name})
        set(LAV_FFMPEG_${lib_name} "${LAV_LIB_${lib_name}}")
    else()
        set(LAV_FFMPEG_FOUND FALSE)
    endif()
endforeach()

set(ffmpeg_message "")
if(NOT LAV_FFMPEG_HEADERS_READY)
    set(ffmpeg_message "FFmpeg has not been configured yet (libavutil/avconfig.h is missing). Build the matching FFmpeg variant (cmake --build <build> --target ffmpeg-msvc-x64, or the x86 / clang targets) or run build_ffmpeg_msvc.sh.")
elseif(NOT LAV_FFMPEG_FOUND)
    set(ffmpeg_message "FFmpeg import libraries were not found in ${LAV_FFMPEG_LIB_DIR}. Build FFmpeg with build_ffmpeg.sh (or build_ffmpeg_msvc.sh) for this architecture first, or set LAV_FFMPEG_LIB_DIR.")
endif()
if(ffmpeg_message)
    if(LAV_REQUIRE_FFMPEG)
        message(FATAL_ERROR "${ffmpeg_message}")
    else()
        message(WARNING "${ffmpeg_message}")
    endif()
endif()

file(GLOB LAV_FFMPEG_DLLS "${LAV_FFMPEG_BIN_DIR}/*-lav-*.dll")

set(LAV_STAGE_DIR "$<IF:$<CONFIG:Debug>,${LAV_DIST_DIR_DEBUG},${LAV_DIST_DIR}>")
add_custom_target(lav_stage_runtime ALL
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${LAV_STAGE_DIR}"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${LAV_ROOT}/resources/LAVFilters.Dependencies.manifest"
        "${LAV_STAGE_DIR}/LAVFilters.Dependencies.manifest"
    COMMENT "Staging the dependency manifest")
if(LAV_FFMPEG_DLLS)
    add_custom_command(TARGET lav_stage_runtime POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${LAV_FFMPEG_DLLS} "${LAV_STAGE_DIR}"
        COMMENT "Staging FFmpeg DLLs")
endif()

# ---------------------------------------------------------------------------
# Intel Media SDK (libmfx). Headers ship in-tree; the import libraries live in
# qsdecoder/MSDK/lib. Without them, QuickSync is omitted and MVC falls back
# to the software decoder.
# ---------------------------------------------------------------------------

set(LAV_MFX_LIB_DIR "${LAV_ROOT}/qsdecoder/MSDK/lib/${LAV_MFX_ARCH}" CACHE PATH
    "Directory containing libmfx.lib")
find_library(LAV_LIBMFX NAMES libmfx PATHS "${LAV_MFX_LIB_DIR}" NO_DEFAULT_PATH)
find_library(LAV_LIBMFX_DEBUG NAMES libmfx_d PATHS "${LAV_MFX_LIB_DIR}" NO_DEFAULT_PATH)
if(LAV_LIBMFX)
    set(LAV_WITH_MFX TRUE)
    if(NOT LAV_LIBMFX_DEBUG)
        set(LAV_LIBMFX_DEBUG "${LAV_LIBMFX}")
    endif()
    message(STATUS "Intel Media SDK: ${LAV_LIBMFX}")
else()
    set(LAV_WITH_MFX FALSE)
    message(WARNING "libmfx.lib was not found in ${LAV_MFX_LIB_DIR}. QuickSync and the MVC decoder will be omitted.")
endif()

# ---------------------------------------------------------------------------
# baseclasses
# ---------------------------------------------------------------------------

add_library(strmbase STATIC
    "${LAV_ROOT}/common/baseclasses/amextra.cpp"
    "${LAV_ROOT}/common/baseclasses/amfilter.cpp"
    "${LAV_ROOT}/common/baseclasses/amvideo.cpp"
    "${LAV_ROOT}/common/baseclasses/arithutil.cpp"
    "${LAV_ROOT}/common/baseclasses/combase.cpp"
    "${LAV_ROOT}/common/baseclasses/cprop.cpp"
    "${LAV_ROOT}/common/baseclasses/ctlutil.cpp"
    "${LAV_ROOT}/common/baseclasses/ddmm.cpp"
    "${LAV_ROOT}/common/baseclasses/dllentry.cpp"
    "${LAV_ROOT}/common/baseclasses/dllsetup.cpp"
    "${LAV_ROOT}/common/baseclasses/mtype.cpp"
    "${LAV_ROOT}/common/baseclasses/outputq.cpp"
    "${LAV_ROOT}/common/baseclasses/perflog.cpp"
    "${LAV_ROOT}/common/baseclasses/pstream.cpp"
    "${LAV_ROOT}/common/baseclasses/pullpin.cpp"
    "${LAV_ROOT}/common/baseclasses/refclock.cpp"
    "${LAV_ROOT}/common/baseclasses/renbase.cpp"
    "${LAV_ROOT}/common/baseclasses/schedule.cpp"
    "${LAV_ROOT}/common/baseclasses/seekpt.cpp"
    "${LAV_ROOT}/common/baseclasses/source.cpp"
    "${LAV_ROOT}/common/baseclasses/strmctl.cpp"
    "${LAV_ROOT}/common/baseclasses/sysclock.cpp"
    "${LAV_ROOT}/common/baseclasses/transfrm.cpp"
    "${LAV_ROOT}/common/baseclasses/transip.cpp"
    "${LAV_ROOT}/common/baseclasses/videoctl.cpp"
    "${LAV_ROOT}/common/baseclasses/vtrans.cpp"
    "${LAV_ROOT}/common/baseclasses/winctrl.cpp"
    "${LAV_ROOT}/common/baseclasses/winutil.cpp"
    "${LAV_ROOT}/common/baseclasses/wxdebug.cpp"
    "${LAV_ROOT}/common/baseclasses/wxlist.cpp"
    "${LAV_ROOT}/common/baseclasses/wxutil.cpp")
target_include_directories(strmbase PUBLIC "${LAV_ROOT}/common/baseclasses")
target_link_libraries(strmbase PUBLIC lav_common strmiids)
lav_configure_static(strmbase)
lav_enable_sse2(strmbase)
set_target_properties(strmbase PROPERTIES OUTPUT_NAME "strmbase" OUTPUT_NAME_DEBUG "strmbasd")

# ---------------------------------------------------------------------------
# DSUtilLite, demuxers, and the DirectShow filters include FFmpeg headers,
# which exist only after FFmpeg's configure step has written avconfig.h.
# ---------------------------------------------------------------------------

if(LAV_FFMPEG_HEADERS_READY)
add_library(DSUtilLite STATIC
    "${LAV_ROOT}/common/DSUtilLite/BaseDSPropPage.cpp"
    "${LAV_ROOT}/common/DSUtilLite/CueSheet.cpp"
    "${LAV_ROOT}/common/DSUtilLite/DSMResourceBag.cpp"
    "${LAV_ROOT}/common/DSUtilLite/MediaSampleSideData.cpp"
    "${LAV_ROOT}/common/DSUtilLite/PopupMenu.cpp"
    "${LAV_ROOT}/common/DSUtilLite/BaseTrayIcon.cpp"
    "${LAV_ROOT}/common/DSUtilLite/ByteParser.cpp"
    "${LAV_ROOT}/common/DSUtilLite/DeCSS/CSSauth.cpp"
    "${LAV_ROOT}/common/DSUtilLite/DeCSS/CSSscramble.cpp"
    "${LAV_ROOT}/common/DSUtilLite/DeCSS/DeCSSInputPin.cpp"
    "${LAV_ROOT}/common/DSUtilLite/DShowUtil.cpp"
    "${LAV_ROOT}/common/DSUtilLite/filterreg.cpp"
    "${LAV_ROOT}/common/DSUtilLite/FontInstaller.cpp"
    "${LAV_ROOT}/common/DSUtilLite/H264Nalu.cpp"
    "${LAV_ROOT}/common/DSUtilLite/locale.cpp"
    "${LAV_ROOT}/common/DSUtilLite/registry.cpp")
target_include_directories(DSUtilLite PUBLIC "${LAV_ROOT}/common/DSUtilLite")
target_link_libraries(DSUtilLite PUBLIC lav_common strmbase)
target_precompile_headers(DSUtilLite PRIVATE "${LAV_ROOT}/common/DSUtilLite/stdafx.h")
lav_configure_static(DSUtilLite)
lav_enable_sse2(DSUtilLite)
set_target_properties(DSUtilLite PROPERTIES OUTPUT_NAME "dsutil" OUTPUT_NAME_DEBUG "dsutild")
endif()

# ---------------------------------------------------------------------------
# libbluray
# ---------------------------------------------------------------------------

add_library(libbluray SHARED
    "${LAV_ROOT}/libbluray/contrib/libudfread/src/default_blockinput.c"
    "${LAV_ROOT}/libbluray/contrib/libudfread/src/ecma167.c"
    "${LAV_ROOT}/libbluray/contrib/libudfread/src/udfread.c"
    "${LAV_ROOT}/libbluray/src/file/dirs_win32.c"
    "${LAV_ROOT}/libbluray/src/file/dir_win32.c"
    "${LAV_ROOT}/libbluray/src/file/dl_win32.c"
    "${LAV_ROOT}/libbluray/src/file/file.c"
    "${LAV_ROOT}/libbluray/src/file/filesystem.c"
    "${LAV_ROOT}/libbluray/src/file/file_win32.c"
    "${LAV_ROOT}/libbluray/src/file/mount.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdj/bdj.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdj/bdjo_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/bdid_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/bdmv_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/clpi_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/extdata_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/index_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/meta_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/mpls_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/navigation.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/sound_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bdnav/uo_mask.c"
    "${LAV_ROOT}/libbluray/src/libbluray/bluray.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/graphics_controller.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/graphics_processor.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/ig_decode.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/m2ts_demux.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/m2ts_filter.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/pes_buffer.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/pg_decode.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/rle.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/textst_decode.c"
    "${LAV_ROOT}/libbluray/src/libbluray/decoders/textst_render.c"
    "${LAV_ROOT}/libbluray/src/libbluray/disc/aacs.c"
    "${LAV_ROOT}/libbluray/src/libbluray/disc/bdplus.c"
    "${LAV_ROOT}/libbluray/src/libbluray/disc/dec.c"
    "${LAV_ROOT}/libbluray/src/libbluray/disc/disc.c"
    "${LAV_ROOT}/libbluray/src/libbluray/disc/properties.c"
    "${LAV_ROOT}/libbluray/src/libbluray/disc/udf_fs.c"
    "${LAV_ROOT}/libbluray/src/libbluray/hdmv/hdmv_vm.c"
    "${LAV_ROOT}/libbluray/src/libbluray/hdmv/mobj_parse.c"
    "${LAV_ROOT}/libbluray/src/libbluray/hdmv/mobj_print.c"
    "${LAV_ROOT}/libbluray/src/libbluray/register.c"
    "${LAV_ROOT}/libbluray/src/util/array.c"
    "${LAV_ROOT}/libbluray/src/util/bits.c"
    "${LAV_ROOT}/libbluray/src/util/event_queue.c"
    "${LAV_ROOT}/libbluray/src/util/logging.c"
    "${LAV_ROOT}/libbluray/src/util/mutex.c"
    "${LAV_ROOT}/libbluray/src/util/refcnt.c"
    "${LAV_ROOT}/libbluray/src/util/strutl.c"
    "${LAV_ROOT}/libbluray/src/util/time.c"
    "${LAV_ROOT}/libbluray/libbluray.def")
target_include_directories(libbluray PUBLIC
    "${LAV_ROOT}/libbluray"
    "${LAV_ROOT}/libbluray/contrib/libudfread/src"
    "${LAV_ROOT}/libbluray/jni"
    "${LAV_ROOT}/libbluray/jni/win32"
    "${LAV_ROOT}/libbluray/src"
    "${LAV_ROOT}/libbluray/src/libbluray")
target_compile_definitions(libbluray PRIVATE
    HAVE_CONFIG_H LIBBLURAY_EXPORTS _CRT_SECURE_NO_WARNINGS)
target_link_libraries(libbluray PRIVATE lav_platform)
lav_configure_shared(libbluray)
lav_enable_sse2(libbluray)
set_target_properties(libbluray PROPERTIES OUTPUT_NAME "libbluray")

# ---------------------------------------------------------------------------
# Demuxers
# ---------------------------------------------------------------------------

if(LAV_FFMPEG_HEADERS_READY)
add_library(demuxers STATIC
    "${LAV_ROOT}/demuxer/Demuxers/BaseDemuxer.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/BDDemuxer.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/ExtradataParser.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/LAVFAudioHelper.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/LAVFDemuxer.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/LAVFInputFormats.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/LAVFVideoHelper.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/LAVFStreamInfo.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/LAVFUtils.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/Packet.cpp"
    "${LAV_ROOT}/demuxer/Demuxers/StreamInfo.cpp")
target_include_directories(demuxers PUBLIC "${LAV_ROOT}/demuxer/Demuxers")
target_link_libraries(demuxers PUBLIC lav_common DSUtilLite libbluray)
target_precompile_headers(demuxers PRIVATE "${LAV_ROOT}/demuxer/Demuxers/stdafx.h")
lav_configure_static(demuxers)
lav_enable_sse2(demuxers)
set_target_properties(demuxers PROPERTIES OUTPUT_NAME "demuxers" OUTPUT_NAME_DEBUG "demuxersd")
endif()

# ---------------------------------------------------------------------------
# Intel Quick Sync decoder
# ---------------------------------------------------------------------------

if(LAV_WITH_MFX)
    add_library(IntelQuickSyncDecoder SHARED
        "${LAV_ROOT}/qsdecoder/base_alllocator.cpp"
        "${LAV_ROOT}/qsdecoder/d3d11_allocator.cpp"
        "${LAV_ROOT}/qsdecoder/d3d11_device.cpp"
        "${LAV_ROOT}/qsdecoder/d3d_device.cpp"
        "${LAV_ROOT}/qsdecoder/QuickSyncExports.cpp"
        "${LAV_ROOT}/qsdecoder/H264Nalu.cpp"
        "${LAV_ROOT}/qsdecoder/QuickSyncDecoder.cpp"
        "${LAV_ROOT}/qsdecoder/d3d_allocator.cpp"
        "${LAV_ROOT}/qsdecoder/frame_constructors.cpp"
        "${LAV_ROOT}/qsdecoder/QuickSync.cpp"
        "${LAV_ROOT}/qsdecoder/QuickSyncUtils.cpp"
        "${LAV_ROOT}/qsdecoder/QuickSyncVPP.cpp"
        "${LAV_ROOT}/qsdecoder/sysmem_allocator.cpp"
        "${LAV_ROOT}/qsdecoder/TimeManager.cpp"
        "${LAV_ROOT}/qsdecoder/stdafx.cpp"
        "${LAV_ROOT}/qsdecoder/IntelQuickSyncDecoder.rc"
        "${LAV_ROOT}/qsdecoder/IntelQuickSyncDecoder.def")
    set_source_files_properties("${LAV_ROOT}/qsdecoder/stdafx.cpp" PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    target_include_directories(IntelQuickSyncDecoder PRIVATE
        "${LAV_ROOT}/qsdecoder"
        "${LAV_ROOT}/qsdecoder/MSDK/include")
    target_compile_definitions(IntelQuickSyncDecoder PRIVATE
        QUICKSYNC_EXPORTS
        $<$<CONFIG:Debug>:DEBUG>
        $<$<NOT:$<CONFIG:Debug>>:_CRT_SECURE_NO_WARNINGS>)
    target_precompile_headers(IntelQuickSyncDecoder PRIVATE "${LAV_ROOT}/qsdecoder/stdafx.h")
    target_link_libraries(IntelQuickSyncDecoder PRIVATE
        winmm uuid D3D9 Dxva2 strmiids d3d11 dxgi
        "$<$<CONFIG:Debug>:${LAV_LIBMFX_DEBUG}>"
        "$<$<NOT:$<CONFIG:Debug>>:${LAV_LIBMFX}>")
    target_link_options(IntelQuickSyncDecoder PRIVATE "/IGNORE:4099")
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        target_compile_options(IntelQuickSyncDecoder PRIVATE "$<$<CONFIG:Release>:/arch:SSE>")
    endif()
    target_compile_options(IntelQuickSyncDecoder PRIVATE "$<$<NOT:$<CONFIG:Debug>>:/GS->")
    lav_configure_shared(IntelQuickSyncDecoder)
    # This project is not part of common.props, so it stays on C++14.
    # std::binary_function, std::bind1st, and std::auto_ptr were removed in C++17.
    set_target_properties(IntelQuickSyncDecoder PROPERTIES
        OUTPUT_NAME "IntelQuickSyncDecoder"
        CXX_STANDARD 14
        CXX_STANDARD_REQUIRED ON)
endif()

# ---------------------------------------------------------------------------
# DirectShow filters. These link the prebuilt FFmpeg import libraries.
# ---------------------------------------------------------------------------

if(LAV_FFMPEG_FOUND AND LAV_FFMPEG_HEADERS_READY)
    set(LAV_SYSTEM_LIBS advapi32 ole32 winmm user32 oleaut32 shell32 Shlwapi Comctl32)

    add_library(LAVSplitter SHARED
        "${LAV_ROOT}/demuxer/LAVSplitter/dllmain.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/InputPin.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/LAVSplitterTrayIcon.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/PacketAllocator.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/SettingsProp.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/PacketQueue.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/OutputPin.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/LAVSplitter.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/StreamParser.cpp"
        "${LAV_ROOT}/demuxer/LAVSplitter/LAVSplitter.rc"
        "${LAV_ROOT}/demuxer/LAVSplitter/LAVSplitter.def"
        "${LAV_ROOT}/demuxer/LAVSplitter/LAVSplitter.manifest")
    target_include_directories(LAVSplitter PRIVATE "${LAV_ROOT}/demuxer/LAVSplitter")
    target_compile_definitions(LAVSplitter PRIVATE LAVSPLITTER_EXPORTS)
    target_precompile_headers(LAVSplitter PRIVATE "${LAV_ROOT}/demuxer/LAVSplitter/stdafx.h")
    target_link_libraries(LAVSplitter PRIVATE
        demuxers DSUtilLite strmbase libbluray
        "${LAV_FFMPEG_avformat}" "${LAV_FFMPEG_avutil}" "${LAV_FFMPEG_avcodec}"
        ${LAV_SYSTEM_LIBS} version)
    lav_configure_shared(LAVSplitter)
    lav_enable_sse2(LAVSplitter)
    set_target_properties(LAVSplitter PROPERTIES OUTPUT_NAME "LAVSplitter" SUFFIX ".ax")
    add_dependencies(LAVSplitter lav_stage_runtime)

    add_library(LAVAudio SHARED
        "${LAV_ROOT}/decoder/LAVAudio/Bitstream.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/BitstreamMAT.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/BitstreamParser.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/dllmain.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/LAVAudio.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/AudioSettingsProp.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/Media.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/parser/dts.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/PostProcessor.cpp"
        "${LAV_ROOT}/decoder/LAVAudio/LAVAudio.rc"
        "${LAV_ROOT}/decoder/LAVAudio/LAVAudio.def"
        "${LAV_ROOT}/decoder/LAVAudio/LAVAudio.manifest")
    target_include_directories(LAVAudio PRIVATE "${LAV_ROOT}/decoder/LAVAudio")
    target_compile_definitions(LAVAudio PRIVATE LAVAUDIO_EXPORTS)
    target_precompile_headers(LAVAudio PRIVATE "${LAV_ROOT}/decoder/LAVAudio/stdafx.h")
    set_source_files_properties("${LAV_ROOT}/decoder/LAVAudio/parser/dts.cpp"
        PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
    target_link_libraries(LAVAudio PRIVATE
        DSUtilLite strmbase
        "${LAV_FFMPEG_avformat}" "${LAV_FFMPEG_avutil}" "${LAV_FFMPEG_avcodec}" "${LAV_FFMPEG_swresample}"
        ${LAV_SYSTEM_LIBS} version)
    lav_configure_shared(LAVAudio)
    lav_enable_sse2(LAVAudio)
    set_target_properties(LAVAudio PROPERTIES OUTPUT_NAME "LAVAudio" SUFFIX ".ax")
    add_dependencies(LAVAudio lav_stage_runtime)

    set(LAV_VIDEO_SOURCES
        "${LAV_ROOT}/decoder/LAVVideo/CCOutputPin.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/avcodec.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/cuvid.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/d3d11va.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/d3d11/D3D11SurfaceAllocator.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/dxva2dec.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/dxva2/DXVA2SurfaceAllocator.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/dxva2/dxva_common.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/pixfmt.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/quicksync.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/wmv9mft.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/DecodeManager.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/dllmain.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/Filtering.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/LAVPixFmtConverter.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/LAVVideo.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/Media.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/parsers/AnnexBConverter.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/parsers/H264SequenceParser.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/parsers/HEVCSequenceParser.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/parsers/MPEG2HeaderParser.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/parsers/VC1HeaderParser.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/convert_direct.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/convert_generic.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/interleave.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/pixconv.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/rgb2rgb_unscaled.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/yuv2rgb.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/yuv2yuv_unscaled.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/yuv420_yuy2.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/pixconv/yuv444_ayuv.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/subtitles/blend/blend_generic.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/subtitles/LAVSubtitleConsumer.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/subtitles/LAVSubtitleFrame.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/subtitles/LAVSubtitleProvider.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/subtitles/LAVVideoSubtitleInputPin.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/subtitles/SubRenderOptionsImpl.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/VideoInputPin.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/VideoOutputPin.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/VideoSettingsProp.cpp"
        "${LAV_ROOT}/decoder/LAVVideo/LAVVideo.rc"
        "${LAV_ROOT}/decoder/LAVVideo/LAVVideo.def"
        "${LAV_ROOT}/decoder/LAVVideo/LAVVideo.manifest")
    if(LAV_WITH_MFX)
        list(APPEND LAV_VIDEO_SOURCES "${LAV_ROOT}/decoder/LAVVideo/decoders/msdk_mvc.cpp")
    else()
        list(APPEND LAV_VIDEO_SOURCES "${LAV_ROOT}/cmake/msdk_mvc_stub.cpp")
    endif()

    add_library(LAVVideo SHARED ${LAV_VIDEO_SOURCES})
    target_include_directories(LAVVideo PRIVATE
        "${LAV_ROOT}/decoder/LAVVideo"
        "${LAV_ROOT}/qsdecoder"
        "${LAV_ROOT}/decoder/LAVVideo/decoders/mvc/include")
    target_compile_definitions(LAVVideo PRIVATE LAVVIDEO_EXPORTS)
    target_precompile_headers(LAVVideo PRIVATE "${LAV_ROOT}/decoder/LAVVideo/stdafx.h")
    target_link_libraries(LAVVideo PRIVATE
        DSUtilLite strmbase
        "${LAV_FFMPEG_avutil}" "${LAV_FFMPEG_avcodec}" "${LAV_FFMPEG_swscale}" "${LAV_FFMPEG_avfilter}"
        ${LAV_SYSTEM_LIBS} gdi32 d3d9 mfuuid dmoguids)
    if(LAV_WITH_MFX)
        target_link_libraries(LAVVideo PRIVATE "${LAV_LIBMFX}")
    endif()
    lav_configure_shared(LAVVideo)
    lav_enable_sse2(LAVVideo)
    set_target_properties(LAVVideo PROPERTIES OUTPUT_NAME "LAVVideo" SUFFIX ".ax")
    add_dependencies(LAVVideo lav_stage_runtime)
endif()
