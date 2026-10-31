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

if [ "$COMPILER" = "clang" ]; then
    if [ "$ARCH" = "x86" ]; then
        # The installed clang-cl is 64-bit hosted and defaults to x64.
        cat > clang-cl-x86.sh << 'EOF'
#!/bin/sh
exec clang-cl -m32 "$@"
EOF
        chmod +x clang-cl-x86.sh
        CC_ARGS="--cc=./clang-cl-x86.sh --cxx=./clang-cl-x86.sh"
    else
        CC_ARGS="--cc=clang-cl --cxx=clang-cl"
    fi
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
