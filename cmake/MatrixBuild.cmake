# Configures and builds the four Release variants:
#   MSVC  x86, MSVC  x64, ClangCL x86, ClangCL x64
# The inner project is this same source tree with LAV_BUILD_ALL left OFF.

find_program(LAV_VSWHERE vswhere
    PATHS
        "$ENV{ProgramFiles\(x86\)}/Microsoft Visual Studio/Installer"
        "$ENV{ProgramFiles}/Microsoft Visual Studio/Installer")
if(NOT LAV_VSWHERE)
    message(FATAL_ERROR "vswhere.exe was not found. Install Visual Studio with the MSVC x86/x64 toolset.")
endif()

execute_process(
    COMMAND "${LAV_VSWHERE}" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
            -property installationVersion
    OUTPUT_VARIABLE LAV_VS_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE LAV_VS_RESULT)
if(NOT LAV_VS_RESULT EQUAL 0 OR LAV_VS_VERSION STREQUAL "")
    message(FATAL_ERROR "Visual Studio with the MSVC x86 and x64 toolsets was not found.")
endif()
string(REGEX MATCH "^[0-9]+" LAV_VS_MAJOR "${LAV_VS_VERSION}")

execute_process(COMMAND "${CMAKE_COMMAND}" --help OUTPUT_VARIABLE LAV_CMAKE_HELP)
string(REGEX MATCH "Visual Studio ${LAV_VS_MAJOR} [0-9]+" LAV_VS_GENERATOR "${LAV_CMAKE_HELP}")
if(NOT LAV_VS_GENERATOR)
    message(FATAL_ERROR
        "CMake has no Visual Studio ${LAV_VS_MAJOR} generator. "
        "Install a CMake build that knows this Visual Studio version.")
endif()

execute_process(
    COMMAND "${LAV_VSWHERE}" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang
            -property installationPath
    OUTPUT_VARIABLE LAV_CLANG_VS
    OUTPUT_STRIP_TRAILING_WHITESPACE)
if(LAV_CLANG_VS STREQUAL "")
    message(FATAL_ERROR
        "The Visual Studio C++ Clang tools are not installed. "
        "Install the \"C++ Clang tools for Windows\" component (toolset ClangCL).")
endif()

message(STATUS "LAV matrix generator: ${LAV_VS_GENERATOR}")

if(POLICY CMP0225)
    cmake_policy(SET CMP0225 NEW)
endif()
include(ExternalProject)

function(lav_add_variant target_name compiler_tag platform toolset)
    set(generator_args
        SOURCE_DIR "${CMAKE_SOURCE_DIR}"
        BINARY_DIR "${CMAKE_BINARY_DIR}/${target_name}"
        CMAKE_GENERATOR "${LAV_VS_GENERATOR}"
        CMAKE_GENERATOR_PLATFORM "${platform}")
    if(NOT toolset STREQUAL "")
        list(APPEND generator_args CMAKE_GENERATOR_TOOLSET "${toolset}")
    endif()

    ExternalProject_Add(${target_name}
        ${generator_args}
        CMAKE_ARGS
            -DLAV_BUILD_ALL=OFF
            -DLAV_COMPILER_TAG=${compiler_tag}
            -DLAV_REQUIRE_FFMPEG=${LAV_REQUIRE_FFMPEG}
        BUILD_COMMAND "${CMAKE_COMMAND}" --build "<BINARY_DIR>" --config Release --parallel
        INSTALL_COMMAND ""
        BUILD_ALWAYS TRUE
        USES_TERMINAL_CONFIGURE TRUE
        USES_TERMINAL_BUILD TRUE)
endfunction()

lav_add_variant(lav-msvc-x86  msvc  Win32 "")
lav_add_variant(lav-msvc-x64  msvc  x64   "")
lav_add_variant(lav-clang-x86 clang Win32 ClangCL)
lav_add_variant(lav-clang-x64 clang x64   ClangCL)

add_custom_target(lav-all
    DEPENDS lav-msvc-x86 lav-msvc-x64 lav-clang-x86 lav-clang-x64)

find_program(LAV_ISCC NAMES ISCC
    HINTS
        "$ENV{ProgramFiles}/Inno Setup 7"
        "$ENV{ProgramFiles\(x86\)}/Inno Setup 7")
if(LAV_ISCC)
    add_custom_target(installer
        COMMAND "${LAV_ISCC}" "${CMAKE_SOURCE_DIR}/LAVFilters.iss"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        DEPENDS lav-msvc-x86 lav-msvc-x64
        COMMENT "Compiling the Inno Setup 7 installer from dist/msvc"
        USES_TERMINAL)
    message(STATUS "Inno Setup 7 compiler: ${LAV_ISCC}")
else()
    add_custom_target(installer
        COMMAND "${CMAKE_COMMAND}" -E echo
            "Inno Setup 7 was not found. Install it from https://jrsoftware.org/isdl.php and reconfigure."
        COMMAND "${CMAKE_COMMAND}" -E false
        COMMENT "Inno Setup 7 is not installed")
    message(STATUS "Inno Setup 7 compiler was not found; the installer target will fail until ISCC.exe is installed")
endif()
