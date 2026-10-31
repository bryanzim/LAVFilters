# Apply the libbluray and qsdecoder patches, build one LAV variant, then
# reverse the patches. Those checkouts are external repositories, so a
# finished build must not leave them modified.
if(NOT LAV_ROOT OR NOT BINARY_DIR)
    message(FATAL_ERROR "run_patched_build.cmake requires LAV_ROOT and BINARY_DIR")
endif()

file(MAKE_DIRECTORY "${LAV_ROOT}/build")
file(LOCK "${LAV_ROOT}/build/.external-patch.lock" GUARD PROCESS TIMEOUT 7200)

function(lav_git_apply repo patch)
    execute_process(
        COMMAND git -C "${repo}" apply --reverse --check "${patch}"
        RESULT_VARIABLE already_applied
        OUTPUT_QUIET
        ERROR_QUIET)
    if(already_applied EQUAL 0)
        return()
    endif()
    execute_process(
        COMMAND git -C "${repo}" apply "${patch}"
        RESULT_VARIABLE apply_rc
        ERROR_VARIABLE apply_err)
    if(NOT apply_rc EQUAL 0)
        message(FATAL_ERROR "Failed to apply ${patch}:\n${apply_err}")
    endif()
endfunction()

function(lav_git_reverse repo patch)
    execute_process(
        COMMAND git -C "${repo}" apply --reverse --check "${patch}"
        RESULT_VARIABLE applied
        OUTPUT_QUIET
        ERROR_QUIET)
    if(NOT applied EQUAL 0)
        return()
    endif()
    execute_process(
        COMMAND git -C "${repo}" apply --reverse "${patch}"
        RESULT_VARIABLE reverse_rc
        ERROR_VARIABLE reverse_err)
    if(NOT reverse_rc EQUAL 0)
        message(FATAL_ERROR "Failed to reverse ${patch}:\n${reverse_err}")
    endif()
endfunction()

set(LAV_EXTERNAL_PATCHES
    "${LAV_ROOT}/libbluray|${LAV_ROOT}/cmake/patches/libbluray/0001-msvc-warning-fixes.patch"
    "${LAV_ROOT}/libbluray/contrib/libudfread|${LAV_ROOT}/cmake/patches/libbluray/0002-libudfread-msvc-warning-fixes.patch"
    "${LAV_ROOT}/qsdecoder|${LAV_ROOT}/cmake/patches/qsdecoder/0001-getversion-deprecated.patch")

foreach(entry IN LISTS LAV_EXTERNAL_PATCHES)
    string(REPLACE "|" ";" entry_parts "${entry}")
    list(GET entry_parts 0 repo)
    list(GET entry_parts 1 patch)
    lav_git_apply("${repo}" "${patch}")
endforeach()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${BINARY_DIR}" --config Release --parallel
    RESULT_VARIABLE lav_build_rc)

foreach(entry IN LISTS LAV_EXTERNAL_PATCHES)
    string(REPLACE "|" ";" entry_parts "${entry}")
    list(GET entry_parts 0 repo)
    list(GET entry_parts 1 patch)
    lav_git_reverse("${repo}" "${patch}")
endforeach()

if(NOT lav_build_rc EQUAL 0)
    message(FATAL_ERROR "LAV build failed with exit code ${lav_build_rc}")
endif()
