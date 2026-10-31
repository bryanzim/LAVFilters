cmake_minimum_required(VERSION 3.24)

# LAV_BUILD_ALL configures four inner builds (MSVC and Clang, x86 and x64).
# Each inner build includes this file again with LAV_BUILD_ALL left OFF.
option(LAV_BUILD_ALL "Build x86 and x64 LAV Filters with both MSVC and Clang" OFF)
option(LAV_REQUIRE_FFMPEG "Require prebuilt FFmpeg import libraries (bin_Win32/lib and bin_x64/lib)" ON)

if(LAV_BUILD_ALL)
    project(LAVFilters LANGUAGES NONE)
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake")
    lav_write_version_rev()
    include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/MatrixBuild.cmake")
    return()
endif()

project(LAVFilters LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Match the Visual Studio projects: static CRT in Release, DLL CRT in Debug.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:DebugDLL>")

if(NOT MSVC)
    message(FATAL_ERROR
        "LAV Filters builds with MSVC or clang-cl (the Visual Studio ClangCL toolset). "
        "Configure with the Visual Studio generator, or pass -T ClangCL for Clang.")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/version.cmake")
lav_write_version_rev()
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/LAVFilters.cmake")
