# Four Release FFmpeg builds: MSVC and Clang, x86 and x64.
# Each one is out of tree. Import libraries and DLLs are copied to the same
# dist/<compiler>/<arch> directory the matching LAV Filters build uses.
# Requires MSYS2 (configure and make), NASM, and the Visual Studio C++ tools.
# Clang uses clang-cl from the Visual Studio ClangCL toolset.

if(LAV_MSYS_BASH STREQUAL "" OR LAV_MSYS_BASH MATCHES "NOTFOUND")
    unset(LAV_MSYS_BASH CACHE)
    unset(LAV_MSYS_BASH)
endif()
find_program(LAV_MSYS_BASH NAMES bash.exe bash
    HINTS
        "C:/msys64/usr/bin"
        "$ENV{MSYS2_ROOT}/usr/bin"
    NO_DEFAULT_PATH)
if(NOT LAV_MSYS_BASH)
    message(FATAL_ERROR
        "MSYS2 bash was not found. Install MSYS2 and make sure usr\\bin\\bash.exe exists "
        "(the default location is C:\\msys64\\usr\\bin\\bash.exe).")
endif()
cmake_path(GET LAV_MSYS_BASH PARENT_PATH LAV_MSYS_BIN)

find_program(LAV_NASM nasm
    HINTS
        "C:/NASM"
        "C:/Program Files/NASM")
if(NOT LAV_NASM)
    message(FATAL_ERROR "nasm.exe was not found. Install NASM and reconfigure.")
endif()
cmake_path(GET LAV_NASM PARENT_PATH LAV_NASM_DIR)

set(LAV_VCVARSALL "${LAV_CLANG_VS}/VC/Auxiliary/Build/vcvarsall.bat")
if(NOT EXISTS "${LAV_VCVARSALL}")
    message(FATAL_ERROR "vcvarsall.bat was not found at ${LAV_VCVARSALL}")
endif()
set(LAV_LLVM_BIN "${LAV_CLANG_VS}/VC/Tools/Llvm/x64/bin")
if(NOT EXISTS "${LAV_LLVM_BIN}/clang-cl.exe")
    message(FATAL_ERROR "clang-cl.exe was not found at ${LAV_LLVM_BIN}")
endif()

message(STATUS "FFmpeg configure shell: ${LAV_MSYS_BASH}")

# compiler: msvc or clang
# vc_arch:  x86 or x64 (vcvarsall argument)
# dist_arch: Win32 or x64 (matches the LAV output directory)
function(lav_add_ffmpeg compiler vc_arch dist_arch)
    set(prefix "${CMAKE_SOURCE_DIR}/dist/${compiler}/${dist_arch}")
    set(build_dir "${CMAKE_BINARY_DIR}/ffmpeg/${compiler}-${vc_arch}")
    file(MAKE_DIRECTORY "${build_dir}")

    set(llvm_path "")
    if(compiler STREQUAL "clang")
        set(llvm_path "set \"PATH=${LAV_LLVM_BIN};%PATH%\"\n")
    endif()
    set(bash_line "\"${LAV_MSYS_BASH}\" --noprofile --norc \"${CMAKE_SOURCE_DIR}/cmake/build_ffmpeg_variant.sh\" ${compiler} ${vc_arch} \"${build_dir}\" \"${prefix}\" \"${CMAKE_SOURCE_DIR}\"")

    set(cmd_file "${build_dir}/build.cmd")
    file(WRITE "${cmd_file}"
"@echo off
call \"${LAV_VCVARSALL}\" ${vc_arch} || exit /b 1
set \"PATH=${LAV_MSYS_BIN};${LAV_NASM_DIR};%PATH%\"
${llvm_path}${bash_line}
exit /b %ERRORLEVEL%
")

    add_custom_target(ffmpeg-${compiler}-${vc_arch}
        COMMAND cmd /c "${cmd_file}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "Building FFmpeg ${compiler} ${vc_arch}"
        USES_TERMINAL
        VERBATIM)

    set(LAV_FFMPEG_${compiler}_${vc_arch}_BUILD "${build_dir}" PARENT_SCOPE)
    set(LAV_FFMPEG_${compiler}_${vc_arch}_PREFIX "${prefix}" PARENT_SCOPE)
endfunction()

lav_add_ffmpeg(msvc  x86 Win32)
lav_add_ffmpeg(msvc  x64 x64)
lav_add_ffmpeg(clang x86 Win32)
lav_add_ffmpeg(clang x64 x64)

add_custom_target(ffmpeg-all
    DEPENDS ffmpeg-msvc-x86 ffmpeg-msvc-x64 ffmpeg-clang-x86 ffmpeg-clang-x64)
