# Writes common/includes/version_rev.h the same way common/genversion.bat does.
# The header is included by the filters and by LAVFilters.iss.
#
# LAV_VERSION_BUILD is the commits on upstream since the last official release.
# origin/master tracks that line. Commits only on this branch are not counted.
# This clone has no upstream tags, so the release base is the newest
# origin/master commit that changed the version macros, which is the
# commit upstream tags.
function(lav_write_version_rev)
    find_package(Git QUIET)
    set(build_number 0)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" rev-parse --verify origin/master
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_QUIET
            ERROR_QUIET
            RESULT_VARIABLE master_rc)
        if(master_rc EQUAL 0)
            set(base "")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" describe --tags --abbrev=0 --match 0.* origin/master
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                OUTPUT_VARIABLE describe_out
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
                RESULT_VARIABLE describe_rc)
            if(describe_rc EQUAL 0 AND NOT describe_out STREQUAL "")
                set(base "${describe_out}")
            else()
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" log -1 --format=%H origin/master
                            -G "define LAV_VERSION_"
                            -- common/includes/version.h
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    OUTPUT_VARIABLE base_out
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    RESULT_VARIABLE base_rc)
                if(base_rc EQUAL 0 AND base_out MATCHES "^[0-9a-fA-F]+$")
                    set(base "${base_out}")
                endif()
            endif()
            if(NOT base STREQUAL "")
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" rev-list --count "${base}..origin/master"
                    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
                    OUTPUT_VARIABLE count_out
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    RESULT_VARIABLE count_rc)
                if(count_rc EQUAL 0 AND count_out MATCHES "^[0-9]+$")
                    set(build_number "${count_out}")
                endif()
            endif()
        endif()
    endif()

    set(header "${CMAKE_SOURCE_DIR}/common/includes/version_rev.h")
    set(contents "#define LAV_VERSION_BUILD ${build_number}\n")
    if(EXISTS "${header}")
        file(READ "${header}" current)
        if(current STREQUAL contents)
            return()
        endif()
    endif()
    file(WRITE "${header}" "${contents}")
endfunction()
