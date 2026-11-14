# SPDX-FileCopyrightText: 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# ArgXVersionHeader.cmake - rewrites the version in a version header in the
# form of include/argx/version.h.
#
# The release scripts include this module and run with "cmake -P". It needs
# nothing that project() sets, and touches no network, git or GitHub.
#
#   argx_version_header_render(<text> <major> <minor> <patch> <prerelease> <out-var>)
#
# Sets <out-var> to <text>, the text of a version header, with the values of
# its four version macros replaced:
#
#   #define ARGX_VERSION_MAJOR <major>
#   #define ARGX_VERSION_MINOR <minor>
#   #define ARGX_VERSION_PATCH <patch>
#   #define ARGX_VERSION_PRERELEASE "<prerelease>"
#
# <prerelease> is the pre-release identifiers without the leading hyphen
# ("rc.1"), or "" for a release. Only the value on each of the four lines
# changes: every other byte of <text> stays as it is, including the blanks
# around the value, a comment after it and the line endings (LF or CRLF).
#
# A value line is a line that starts, after optional blanks, with
# "#define ARGX_VERSION_<PART>" (blanks are allowed around "#") followed by a
# blank or the end of the line, and that does not start inside a block
# comment. Lines inside a comment, such as " * #define ..." or
# "// #define ...", are not value lines. The value is the token after the
# name: for the three numbers, the characters up to the next blank or "/";
# for the pre-release, a string literal.
#
# Stops with a FATAL_ERROR that starts with "error: " and holds every reason
# when the version <major>.<minor>.<patch>[-<prerelease>] is not valid for
# ArgXSemver.cmake, or when the value line of a macro is not found exactly
# once or has no value of its form. The text is then not rewritten at all.
#
#   argx_version_header_write(<header-path> <major> <minor> <patch> <prerelease>)
#
# Reads the header at <header-path>, rewrites it with
# argx_version_header_render() and writes it back. Then reads it again with
# argx_read_version() (cmake/ArgXVersion.cmake) and stops with a
# FATAL_ERROR unless it gives the version just written. The file is not
# written when the rewrite fails. A relative <header-path> is taken from
# CMAKE_CURRENT_SOURCE_DIR. The file is read and written byte for byte with
# ArgXReadBytes.cmake, since file(READ) drops the CR of a CRLF line ending
# and, on Windows, file(WRITE) turns LF into CRLF; so its line endings are
# kept too. On Windows, a header that does not end with a line ending cannot
# be written exactly, and stops the rewrite with a FATAL_ERROR before the file
# is written.

include_guard(GLOBAL)

include("${CMAKE_CURRENT_LIST_DIR}/ArgXSemver.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ArgXReadBytes.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ArgXVersion.cmake")

# The functions keep the policies in effect here, whatever the caller has set
# (a "cmake -P" script may have set none).
cmake_policy(PUSH)
cmake_policy(VERSION 3.21)

# Sets <out-var> to TRUE when the end of <line> is inside a block comment,
# given whether its start is (<in-comment>). A "//" outside a block comment
# ends the line for this purpose.
function(_argx_version_header_comment_state line in_comment out_var)
    set(rest "${line}")
    set(state ${in_comment})
    while(TRUE)
        if(state)
            string(FIND "${rest}" "*/" pos)
            if(pos EQUAL -1)
                break()
            endif()
            math(EXPR pos "${pos} + 2")
            string(SUBSTRING "${rest}" ${pos} -1 rest)
            set(state FALSE)
        else()
            string(FIND "${rest}" "/*" block_pos)
            string(FIND "${rest}" "//" line_pos)
            if(block_pos EQUAL -1 OR (NOT line_pos EQUAL -1 AND line_pos LESS block_pos))
                break()
            endif()
            math(EXPR pos "${block_pos} + 2")
            string(SUBSTRING "${rest}" ${pos} -1 rest)
            set(state TRUE)
        endif()
    endwhile()
    set(${out_var} ${state} PARENT_SCOPE)
endfunction()

function(argx_version_header_render text major minor patch prerelease out_var)
    # The version must be valid before anything is rewritten. Each number is
    # checked on its own, so that a number such as "1.2" is not taken for two.
    set(problems "")
    foreach(part IN ITEMS major minor patch)
        if(NOT "${${part}}" MATCHES "^[0-9]+$")
            string(APPEND problems " the ${part} version \"${${part}}\" is not a number.")
        endif()
    endforeach()
    if(problems STREQUAL "")
        set(version "${major}.${minor}.${patch}")
        if(NOT prerelease STREQUAL "")
            string(APPEND version "-${prerelease}")
        endif()
        argx_semver_parse("${version}" semver)
        if(NOT semver_VALID)
            set(problems " ${semver_ERROR}")
        endif()
    endif()
    if(NOT problems STREQUAL "")
        message(FATAL_ERROR "error: cannot rewrite the version header:${problems}")
    endif()

    set(MAJOR_value "${major}")
    set(MINOR_value "${minor}")
    set(PATCH_value "${patch}")
    set(PRERELEASE_value "\"${prerelease}\"")
    foreach(part IN ITEMS MAJOR MINOR PATCH PRERELEASE)
        set(${part}_count 0)
        set(${part}_bad FALSE)
    endforeach()

    # Walk the text line by line. A line is the text up to, not including, its
    # "\n"; a CR before the "\n" stays at the end of the line and is kept.
    set(result "")
    set(rest "${text}")
    set(in_comment FALSE)
    while(NOT rest STREQUAL "")
        string(FIND "${rest}" "\n" newline_pos)
        if(newline_pos EQUAL -1)
            set(line "${rest}")
            set(ending "")
            set(rest "")
        else()
            string(SUBSTRING "${rest}" 0 ${newline_pos} line)
            set(ending "\n")
            math(EXPR after "${newline_pos} + 1")
            string(SUBSTRING "${rest}" ${after} -1 rest)
        endif()

        if(NOT in_comment AND line MATCHES
                "^[ \t]*#[ \t]*define[ \t]+ARGX_VERSION_(MAJOR|MINOR|PATCH|PRERELEASE)([ \t\r]|$)")
            set(part "${CMAKE_MATCH_1}")
            math(EXPR ${part}_count "${${part}_count} + 1")

            # The head is the line up to the end of the macro name.
            string(REGEX MATCH "^[ \t]*#[ \t]*define[ \t]+ARGX_VERSION_[A-Z]+" head "${line}")
            string(LENGTH "${head}" head_length)
            string(SUBSTRING "${line}" ${head_length} -1 tail)
            if(part STREQUAL "PRERELEASE")
                set(value_regex "^([ \t]+)(\"[^\"]*\")")
            else()
                set(value_regex "^([ \t]+)([^ \t\r/]+)")
            endif()
            string(REGEX MATCH "${value_regex}" blanks_and_value "${tail}")
            if(blanks_and_value STREQUAL "")
                set(${part}_bad TRUE)
                string(STRIP "${tail}" ${part}_found)
            else()
                string(REGEX REPLACE "[^ \t].*$" "" blanks "${blanks_and_value}")
                string(LENGTH "${blanks_and_value}" matched_length)
                string(SUBSTRING "${tail}" ${matched_length} -1 after_value)
                set(line "${head}${blanks}${${part}_value}${after_value}")
            endif()
        endif()

        _argx_version_header_comment_state("${line}" ${in_comment} in_comment)
        string(APPEND result "${line}${ending}")
    endwhile()

    foreach(part IN ITEMS MAJOR MINOR PATCH PRERELEASE)
        set(macro "ARGX_VERSION_${part}")
        if(${part}_count EQUAL 0)
            string(APPEND problems "\n  ${macro} was not found.")
        elseif(${part}_count GREATER 1)
            string(APPEND problems "\n  ${macro} is defined more than once.")
        elseif(${part}_bad)
            string(REGEX REPLACE "\r$" "" found "${${part}_found}")
            if(found STREQUAL "")
                string(APPEND problems "\n  ${macro} has no value.")
            elseif(part STREQUAL "PRERELEASE")
                string(APPEND problems "\n  ${macro} is ${found}, which is not a string literal.")
            else()
                string(APPEND problems "\n  ${macro} has no value that can be replaced: ${found}")
            endif()
        endif()
    endforeach()
    if(NOT problems STREQUAL "")
        message(FATAL_ERROR
            "error: cannot rewrite the version header:${problems}\n"
            "Each macro must be defined exactly once, on one line that is not in a comment, "
            "in one of the forms\n"
            "  #define ARGX_VERSION_<PART> <integer>\n"
            "  #define ARGX_VERSION_PRERELEASE \"<identifiers>\"")
    endif()

    set(${out_var} "${result}" PARENT_SCOPE)
endfunction()

function(argx_version_header_write header major minor patch prerelease)
    get_filename_component(path "${header}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}")
        message(FATAL_ERROR
            "error: cannot rewrite the version header ${path}: the file does not exist.")
    endif()

    argx_read_bytes("${path}" "the version header" text)
    argx_version_header_render("${text}" "${major}" "${minor}" "${patch}" "${prerelease}" result)
    argx_write_bytes("${path}" "${result}" "the version header")

    # Read the header back as the build reads it.
    argx_read_version("${path}" version read_prerelease)
    if(NOT version STREQUAL "${major}.${minor}.${patch}" OR NOT read_prerelease STREQUAL prerelease)
        message(FATAL_ERROR
            "error: the version header ${path} was rewritten, but it reads as "
            "\"${version}\" with the pre-release \"${read_prerelease}\" instead of "
            "\"${major}.${minor}.${patch}\" with the pre-release \"${prerelease}\".")
    endif()
endfunction()

cmake_policy(POP)
