# ClargsVersion.cmake - reads the CLArgs version from the public header.
#
# The public header include/clargs/version.h is the single source of the
# version. This module reads it as text, so the build, the package files and
# the library all use the same numbers.
#
#   clargs_read_version(<header-path> <out-var>)
#
# Sets <out-var> to "<major>.<minor>.<patch>".
#
# <header-path> must contain each of CLARGS_VERSION_MAJOR, CLARGS_VERSION_MINOR
# and CLARGS_VERSION_PATCH exactly once, on one line, in the form
#
#   #define CLARGS_VERSION_<PART> <integer>
#
# where <integer> is a decimal literal in the range 0-255 without a sign, a
# suffix or leading zeros (the compiler reads a leading zero as octal). Blanks
# around the tokens and comments are ignored, as the compiler ignores them. A
# relative <header-path> is taken from CMAKE_CURRENT_SOURCE_DIR.
#
# Stops with a FATAL_ERROR that names the header and every macro at fault if
# the header cannot be read, a macro is missing or defined more than once, or
# a value is not such an integer.
#
# Does not depend on anything project() sets: call it before project(), or
# from a script run with "cmake -P".

include_guard(GLOBAL)

# The function keeps the policies in effect here, whatever the caller has set
# (a "cmake -P" script may have set none).
cmake_policy(PUSH)
cmake_policy(VERSION 3.21)

function(clargs_read_version header out_var)
    if(NOT ARGC EQUAL 2)
        message(FATAL_ERROR
            "clargs_read_version: expected 2 arguments (<header-path> <out-var>), got ${ARGC}.")
    endif()

    get_filename_component(path "${header}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        message(FATAL_ERROR
            "clargs_read_version: cannot read the version from\n"
            "  ${path}\n"
            "The file does not exist.")
    endif()

    file(READ "${path}" rest)
    string(REPLACE "\r\n" "\n" rest "${rest}")
    string(REPLACE "\r" "\n" rest "${rest}")

    # Drop comments, so that a macro inside a comment does not count.
    set(text "")
    while(TRUE)
        string(FIND "${rest}" "/*" block_pos)
        string(FIND "${rest}" "//" line_pos)
        if(block_pos EQUAL -1 AND line_pos EQUAL -1)
            break()
        endif()
        if(line_pos EQUAL -1 OR (NOT block_pos EQUAL -1 AND block_pos LESS line_pos))
            # A block comment becomes one blank. It ends at the first "*/".
            string(SUBSTRING "${rest}" 0 ${block_pos} head)
            math(EXPR after "${block_pos} + 2")
            string(SUBSTRING "${rest}" ${after} -1 rest)
            string(FIND "${rest}" "*/" end_pos)
            if(end_pos EQUAL -1)
                set(rest "")
            else()
                math(EXPR after "${end_pos} + 2")
                string(SUBSTRING "${rest}" ${after} -1 rest)
            endif()
            string(APPEND text "${head} ")
        else()
            # A line comment ends before the end of the line.
            string(SUBSTRING "${rest}" 0 ${line_pos} head)
            string(SUBSTRING "${rest}" ${line_pos} -1 rest)
            string(FIND "${rest}" "\n" end_pos)
            if(end_pos EQUAL -1)
                set(rest "")
            else()
                string(SUBSTRING "${rest}" ${end_pos} -1 rest)
            endif()
            string(APPEND text "${head}")
        endif()
    endwhile()
    string(APPEND text "${rest}")

    # Give every line its own newline on both sides, so that one pattern
    # matches a whole line wherever the line is.
    string(REPLACE "\n" "\n\n" text "\n${text}\n")

    set(problems "")
    set(version "")
    foreach(part IN ITEMS MAJOR MINOR PATCH)
        set(macro "CLARGS_VERSION_${part}")
        set(line_regex "\n[ \t]*#[ \t]*define[ \t]+${macro}([ \t][^\n]*)?\n")

        string(REGEX MATCH "${line_regex}" line "${text}")
        if(line STREQUAL "")
            string(APPEND problems "  ${macro} was not found.\n")
            continue()
        endif()
        string(STRIP "${CMAKE_MATCH_1}" value)

        string(FIND "${text}" "${line}" line_pos)
        string(LENGTH "${line}" line_length)
        math(EXPR after "${line_pos} + ${line_length}")
        string(SUBSTRING "${text}" ${after} -1 rest)
        if(rest MATCHES "${line_regex}")
            string(APPEND problems "  ${macro} is defined more than once.\n")
            continue()
        endif()

        # At most three digits, so the comparison below never sees a huge number.
        if(NOT value MATCHES "^(0|[1-9][0-9]?[0-9]?)$" OR value GREATER 255)
            string(APPEND problems
                "  ${macro} is \"${value}\", which is not an integer in the range 0-255.\n")
            continue()
        endif()

        if(NOT part STREQUAL "MAJOR")
            string(APPEND version ".")
        endif()
        string(APPEND version "${value}")
    endforeach()

    if(NOT problems STREQUAL "")
        message(FATAL_ERROR
            "clargs_read_version: cannot read the version from\n"
            "  ${path}\n"
            "${problems}"
            "Each macro must be on one line, in the form\n"
            "  #define CLARGS_VERSION_<PART> <integer>\n"
            "where <integer> is in the range 0-255, in decimal, without leading zeros.")
    endif()

    set(${out_var} "${version}" PARENT_SCOPE)
endfunction()

cmake_policy(POP)
