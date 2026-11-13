# ClargsReadBytes.cmake - reads a text file byte for byte, line endings
# included.
#
# The release scripts include this module and run with "cmake -P". It needs
# nothing that project() sets, and touches no network, git or GitHub.
#
#   clargs_read_bytes(<path> <what> <out-var>)
#
# Sets <out-var> to the bytes of the file <path>, with every CR kept: file(READ)
# without HEX drops the CR of a CRLF line ending, so the file is read in
# hexadecimal and turned back into a string. <what> names the file in the
# error, such as "the changelog". A file that holds a NUL byte cannot be a
# string, and stops the script with a FATAL_ERROR that starts with "error: ".

include_guard(GLOBAL)

# The function keeps the policies in effect here, whatever the caller has set
# (a "cmake -P" script may have set none).
cmake_policy(PUSH)
cmake_policy(VERSION 3.21)

function(clargs_read_bytes path what out_var)
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

cmake_policy(POP)
