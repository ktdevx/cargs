# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# ArgXReadBytes.cmake - reads and writes a text file byte for byte, line
# endings included.
#
# The release scripts include this module and run with "cmake -P". It needs
# nothing that project() sets, and touches no network, git or GitHub.
#
#   argx_read_bytes(<path> <what> <out-var>)
#
# Sets <out-var> to the bytes of the file <path>, with every CR kept: file(READ)
# without HEX drops the CR of a CRLF line ending, so the file is read in
# hexadecimal and turned back into a string. <what> names the file in the
# error, such as "the changelog". A file that holds a NUL byte cannot be a
# string, and stops the script with a FATAL_ERROR that starts with "error: ".
#
#   argx_write_bytes(<path> <content> <what>)
#
# Writes <content> to the file <path> exactly, byte for byte, creating the
# directories above it. A relative <path> is taken from
# CMAKE_CURRENT_SOURCE_DIR. On Windows, file(WRITE) writes in text mode, so
# every LF becomes CRLF and a CRLF becomes CR CR LF. There the content is
# written with file(CONFIGURE ... NEWLINE_STYLE LF) instead, which writes in
# binary mode: "@" and "cmakedefine" are escaped so that nothing is replaced,
# and the CR of each CRLF is doubled, since file(CONFIGURE) drops one CR before
# each LF. Any line endings can be written this way, LF, CRLF or a mix, but
# file(CONFIGURE) ends the last line with a line ending, so on Windows a
# content that is not empty must end with LF. Elsewhere, file(WRITE) is used,
# which keeps every byte.
#
# The file is read back with argx_read_bytes() and compared with <content>.
# A content that cannot be written exactly, or a file that does not read back
# as <content>, stops the script with a FATAL_ERROR that starts with "error: "
# and names <what> and the path.
#
#   argx_check_write_bytes(<path> <content> <what>)
#
# Stops the script with the FATAL_ERROR of argx_write_bytes() when <content>
# cannot be written exactly to <path> on this host, and does nothing
# otherwise. A script that writes several files calls it for each of them
# first, so that it writes either all of them or none.
#
# Setting ARGX_WRITE_BYTES_WITH_CONFIGURE to a true value makes these
# functions behave as on Windows on any host, so that the tests can check the
# way Windows writes on Linux and macOS too.

include_guard(GLOBAL)

# The functions keep the policies in effect here, whatever the caller has set
# (a "cmake -P" script may have set none).
cmake_policy(PUSH)
cmake_policy(VERSION 3.21)

function(argx_read_bytes path what out_var)
    file(READ "${path}" hex HEX)
    set(text "")
    if(NOT hex STREQUAL "")
        string(REGEX MATCHALL ".." bytes "${hex}")
        set(codes "")
        foreach(byte IN LISTS bytes)
            if(byte STREQUAL "00")
                message(FATAL_ERROR "error: cannot read ${what} ${path}: the file holds a NUL byte.")
            endif()
            math(EXPR code "0x${byte}")
            list(APPEND codes ${code})
        endforeach()
        string(ASCII ${codes} text)
    endif()
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to TRUE when files are written with file(CONFIGURE).
function(_argx_write_bytes_with_configure out_var)
    if(CMAKE_HOST_WIN32 OR ARGX_WRITE_BYTES_WITH_CONFIGURE)
        set(${out_var} TRUE PARENT_SCOPE)
    else()
        set(${out_var} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(argx_check_write_bytes path content what)
    _argx_write_bytes_with_configure(with_configure)
    if(with_configure AND NOT content STREQUAL "" AND NOT content MATCHES "\n$")
        message(FATAL_ERROR
            "error: cannot write ${what} ${path} byte for byte: on Windows, a text that is "
            "not empty can be written exactly only when it ends with a line ending (LF or CRLF), "
            "and this one does not.")
    endif()
endfunction()

function(argx_write_bytes path content what)
    get_filename_component(path "${path}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
    argx_check_write_bytes("${path}" "${content}" "${what}")
    _argx_write_bytes_with_configure(with_configure)
    if(NOT with_configure OR content STREQUAL "")
        # A content without LF is written as it is even in text mode.
        file(WRITE "${path}" "${content}")
    else()
        get_filename_component(dir "${path}" DIRECTORY)
        file(MAKE_DIRECTORY "${dir}")
        # "@" first, so that the "@" of the escapes that follow stay as they are.
        string(REPLACE "@" "@argx_at@" escaped "${content}")
        string(REPLACE "cmakedefine" "@argx_cmakedefine@" escaped "${escaped}")
        string(REPLACE "\r\n" "\r\r\n" escaped "${escaped}")
        set(argx_at "@")
        set(argx_cmakedefine "cmakedefine")
        file(CONFIGURE OUTPUT "${path}" CONTENT "${escaped}" @ONLY NEWLINE_STYLE LF)
    endif()

    argx_read_bytes("${path}" "${what}" written)
    if(NOT written STREQUAL content)
        message(FATAL_ERROR
            "error: ${what} ${path} was written, but its bytes differ from the text to write.")
    endif()
endfunction()

cmake_policy(POP)
