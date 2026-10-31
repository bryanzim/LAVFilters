#!/bin/sh
# Out-of-tree Release FFmpeg build for one compiler and one architecture.
# Invoked from a Visual Studio environment (vcvarsall already ran):
#   build_ffmpeg_variant.sh <msvc|clang> <x86|x64> <build-dir> <prefix> <source-root>
set -e

COMPILER=$1
ARCH=$2
BUILD_DIR=$3
PREFIX=$4
SOURCE_ROOT=$5

if [ -z "$SOURCE_ROOT" ]; then
    echo "usage: build_ffmpeg_variant.sh <msvc|clang> <x86|x64> <build-dir> <prefix> <source-root>"
    exit 1
fi

# Keep cl.exe arguments as Windows paths. MSYS must not rewrite -I or -Fo.
export MSYS2_ARG_CONV_EXCL='*'
export MSYS_NO_PATHCONV=1

to_unix() {
    cygpath -u "$1"
}

BUILD_U=$(to_unix "$BUILD_DIR")
PREFIX_U=$(to_unix "$PREFIX")
ROOT_U=$(to_unix "$SOURCE_ROOT")

case "$ARCH" in
    x64)
        FFARCH=x86_64
        INC=64
        ;;
    x86)
        FFARCH=x86
        INC=32
        ;;
    *)
        echo "Unknown architecture: $ARCH"
        exit 1
        ;;
esac

case "$COMPILER" in
    msvc|clang)
        ;;
    *)
        echo "Unknown compiler: $COMPILER"
        exit 1
        ;;
esac

mkdir -p "$BUILD_U" "$PREFIX_U/lib"
cd "$BUILD_U"

CC_ARGS=""
# Release matches build_ffmpeg_msvc.sh: dynamic CRT inside the FFmpeg DLLs.
EXTRA_CFLAGS="-D_WIN32_WINNT=0x0601 -DWINVER=0x0601 -Zo -GS- -MD -I${SOURCE_ROOT}/thirdparty/${INC}/include"
EXTRA_LDFLAGS="-NODEFAULTLIB:libcmt -LIBPATH:${SOURCE_ROOT}/thirdparty/${INC}/lib"

ZCC="cl"
if [ "$COMPILER" = "clang" ]; then
    if [ "$ARCH" = "x86" ]; then
        # The installed clang-cl is 64-bit hosted and defaults to x64.
        # FFmpeg's mscl.exe launches the compiler with CreateProcess, which
        # cannot run a shell script, so this is a real executable.
        cat > clang-cl-x86.c << 'EOF'
#include <process.h>
#include <stddef.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    char **args = calloc((size_t)argc + 2, sizeof(*args));
    int i;
    intptr_t rc;

    if (!args)
        return 1;
    args[0] = "clang-cl";
    args[1] = "-m32";
    for (i = 1; i < argc; i++)
        args[i + 1] = argv[i];
    args[argc + 1] = NULL;
    rc = _spawnvp(_P_WAIT, "clang-cl", (const char * const *)args);
    free(args);
    return rc < 0 ? 1 : (int)rc;
}
EOF
        cl /nologo /O2 /MD /Fe:clang-cl-x86.exe /Foclang-cl-x86.obj clang-cl-x86.c
        CC_ARGS="--cc=./clang-cl-x86.exe --cxx=./clang-cl-x86.exe"
        ZCC="./clang-cl-x86.exe"
    else
        CC_ARGS="--cc=clang-cl --cxx=clang-cl"
        ZCC="clang-cl"
    fi
fi

# Upstream enables zlib, and this tree does not ship zlib headers or an MSVC lib.
ZLIB_SRC_WIN="${SOURCE_ROOT}/build/zlib-1.3.1"
ZLIB_SRC_U=$(to_unix "$ZLIB_SRC_WIN")
ZLIB_OUT_WIN="${BUILD_DIR}/zlib"
ZLIB_OUT_U=$(to_unix "$ZLIB_OUT_WIN")
if [ ! -f "$ZLIB_SRC_U/zlib.h" ]; then
    ZLIB_TGZ_U=$(to_unix "${SOURCE_ROOT}/build/zlib-1.3.1.tar.gz")
    mkdir -p "$(dirname "$ZLIB_TGZ_U")"
    if [ ! -f "$ZLIB_TGZ_U" ]; then
        curl -fsSL -o "$ZLIB_TGZ_U" \
            "https://github.com/madler/zlib/releases/download/v1.3.1/zlib-1.3.1.tar.gz"
    fi
    tar -C "$(dirname "$ZLIB_TGZ_U")" -xzf "$ZLIB_TGZ_U"
fi
# FFmpeg's config.h defines HAVE_UNISTD_H as 0. zlib treats any definition
# as "the header exists" and then includes unistd.h, which the Windows SDK
# does not provide.
sed -i 's/^#ifdef HAVE_UNISTD_H$/#if defined(HAVE_UNISTD_H) \&\& HAVE_UNISTD_H/' \
    "$ZLIB_SRC_U/zconf.h"
if [ ! -f "$ZLIB_OUT_U/zlib.lib" ]; then
    mkdir -p "$ZLIB_OUT_U"
    for zsrc in adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c \
        gzread.c gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c \
        uncompr.c zutil.c; do
        "$ZCC" /nologo /MD /O2 /c /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE \
            "/I${ZLIB_SRC_WIN}" \
            "/Fo${ZLIB_OUT_WIN}/${zsrc%.c}.obj" \
            "${ZLIB_SRC_WIN}/${zsrc}"
    done
    (cd "$ZLIB_OUT_U" && lib.exe /nologo /out:zlib.lib ./*.obj)
fi
EXTRA_CFLAGS="${EXTRA_CFLAGS} -I${ZLIB_SRC_WIN}"
EXTRA_LDFLAGS="${EXTRA_LDFLAGS} -LIBPATH:${ZLIB_OUT_WIN}"

# clang-cl does not define __GNUC__, so the GCC target pragmas in the HEVC
# intrinsics never enable SSE and the clang build fails. FFmpeg is an external
# checkout, so that change is a patch applied here instead of a fork commit.
# All four variants share that checkout, so the apply is serialized.
# cygpath -m: Git for Windows does not accept an MSYS /f/ path, and the
# build disables MSYS path conversion.
FFMPEG_REPO=$(cygpath -m "$ROOT_U/ffmpeg")
FFMPEG_PATCH=$(cygpath -m "$ROOT_U/cmake/patches/0001-clang-cl-hevc-intrinsic-targets.patch")
PATCH_LOCK="$ROOT_U/build/.ffmpeg-patch.lock"
mkdir -p "$(dirname "$PATCH_LOCK")"
while ! mkdir "$PATCH_LOCK" 2>/dev/null; do
    sleep 1
done
# Always put the checkout back, including when configure or make fails,
# so Explorer does not keep a modified ffmpeg folder.
restore_ffmpeg_patch() {
    status=$?
    if git -C "$FFMPEG_REPO" apply --reverse --check "$FFMPEG_PATCH" >/dev/null 2>&1; then
        git -C "$FFMPEG_REPO" apply --reverse "$FFMPEG_PATCH" || true
    fi
    rmdir "$PATCH_LOCK" 2>/dev/null || true
    exit "$status"
}
trap restore_ffmpeg_patch EXIT
if ! git -C "$FFMPEG_REPO" apply --reverse --check "$FFMPEG_PATCH" >/dev/null 2>&1; then
    git -C "$FFMPEG_REPO" apply "$FFMPEG_PATCH"
fi

if [ ! -f config.h ]; then
    # shellcheck disable=SC2086
    "$ROOT_U/ffmpeg/configure" \
        --toolchain=msvc \
        $CC_ARGS \
        --extra-cflags="${EXTRA_CFLAGS}" \
        --extra-ldflags="${EXTRA_LDFLAGS}" \
        --enable-shared \
        --disable-static \
        --enable-gpl \
        --enable-version3 \
        --disable-autodetect \
        --enable-w32threads \
        --disable-demuxer=matroska \
        --disable-filters \
        --enable-filter=scale,yadif,w3fdif,bwdif \
        --disable-protocol=async,cache,concat,httpproxy,icecast,md5,subfile \
        --disable-muxers \
        --enable-muxer=spdif \
        --disable-bsfs \
        --enable-bsf=extract_extradata,dovi_split \
        --disable-avdevice \
        --disable-encoders \
        --disable-devices \
        --disable-programs \
        --disable-doc \
        --enable-avisynth \
        --enable-d3d11va \
        --enable-dxva2 \
        --enable-schannel \
        --enable-zlib \
        --build-suffix=-lav \
        --disable-stripping \
        --disable-debug \
        --arch="${FFARCH}" \
        > config.out 2>&1 || {
            cat config.out
            exit 1
        }
    cat config.out
fi

# FFmpeg appends -W3 after --extra-cflags, so /w has to land at the end of
# CFLAGS. Upstream diagnostics stay out of the LAV build log. cl warns D9025
# when -W3 and /w are both present, so the MSVC builds drop -W3.
if [ -f ffbuild/config.mak ]; then
    if ! grep -q LAV_WARN_SILENCE ffbuild/config.mak; then
        sed -i 's/^\(CFLAGS=.*\)$/\1 \/w/' ffbuild/config.mak
        sed -i 's/^\(CXXFLAGS=.*\)$/\1 \/w/' ffbuild/config.mak
        printf '\n# LAV_WARN_SILENCE\n' >> ffbuild/config.mak
    fi
    if [ "$COMPILER" = "msvc" ]; then
        sed -i 's/ -W3 / /' ffbuild/config.mak
    fi
fi

make -j"${NUMBER_OF_PROCESSORS:-4}"

copied=0
for dll in lib*/*-lav-*.dll; do
    if [ -f "$dll" ]; then
        cp -u "$dll" "$PREFIX_U/"
        copied=$((copied + 1))
    fi
done
for pdb in lib*/*-lav-*.pdb; do
    if [ -f "$pdb" ]; then
        cp -u "$pdb" "$PREFIX_U/"
    fi
done
for lib in lib*/*.lib; do
    if [ -f "$lib" ]; then
        cp -u "$lib" "$PREFIX_U/lib/"
    fi
done

if [ "$copied" -eq 0 ]; then
    echo "FFmpeg produced no *-lav-*.dll files in $BUILD_DIR"
    exit 1
fi
