# Writes common/includes/version_rev.h the same way common/genversion.bat does.
# The header is included by the filters and by LAVFilters.iss.
function(lav_write_version_rev)
    find_package(Git QUIET)
    set(build_number 0)
    if(GIT_FOUND)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" describe --long --abbrev=5 HEAD
            WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
            OUTPUT_VARIABLE describe_out
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
            RESULT_VARIABLE describe_rc)
        if(describe_rc EQUAL 0 AND describe_out MATCHES "-([0-9]+)-g[0-9a-fA-F]+$")
            set(build_number "${CMAKE_MATCH_1}")
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
