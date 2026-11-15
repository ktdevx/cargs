# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# check_readme.cmake - checks that the example of the README is the program
# examples/greet/greet.c, and that it prints what the README shows.
#
#   cmake -D ARGX_README=<README.md> -D ARGX_SOURCE=<greet.c>
#         -D ARGX_PROGRAM=<built greet> -P check_readme.cmake
#
# In the section "## Example" of the README:
#
#   - The first block that starts with "```c" is the code. It must equal
#     greet.c after the comment at its top and the empty line that follows
#     the comment.
#   - The first block that starts with "```" after the code is the output.
#     Each line that starts with "$ greet" is a command line; the lines up to
#     the empty line before the next command line, or to the end of the
#     block, are its output. The program is run with the arguments of the
#     command line, split at blanks, and its standard output and standard
#     error stream together must equal that output.
#
# Every difference is reported, and the script fails if there is one. Line
# endings are compared as LF.

foreach(variable ARGX_README ARGX_SOURCE ARGX_PROGRAM)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "check_readme.cmake: ${variable} is not set")
    endif()
endforeach()

file(READ "${ARGX_README}" readme)
file(READ "${ARGX_SOURCE}" source)
string(REPLACE "\r\n" "\n" readme "${readme}")
string(REPLACE "\r\n" "\n" source "${source}")

# The section "## Example", up to the next section.
string(FIND "${readme}" "\n## Example\n" start)
if(start EQUAL -1)
    message(FATAL_ERROR "check_readme.cmake: the README has no section \"## Example\"")
endif()
string(SUBSTRING "${readme}" ${start} -1 section)
string(SUBSTRING "${section}" 1 -1 section)
string(FIND "${section}" "\n## " end)
if(NOT end EQUAL -1)
    string(SUBSTRING "${section}" 0 ${end} section)
endif()

# Returns in out_body the text of the first block that starts with opening
# in text, without its fences, and in out_rest the text after the block.
function(argx_take_block text opening out_body out_rest)
    string(FIND "${text}" "\n${opening}\n" open)
    if(open EQUAL -1)
        message(FATAL_ERROR "check_readme.cmake: no block \"${opening}\" in the section \"## Example\"")
    endif()
    string(LENGTH "\n${opening}\n" opening_length)
    math(EXPR body_start "${open} + ${opening_length}")
    string(SUBSTRING "${text}" ${body_start} -1 body)
    string(FIND "${body}" "\n```\n" close)
    if(close EQUAL -1)
        message(FATAL_ERROR "check_readme.cmake: the block \"${opening}\" is not closed")
    endif()
    math(EXPR body_length "${close} + 1")
    math(EXPR rest_start "${close} + 5")
    string(SUBSTRING "${body}" ${rest_start} -1 rest)
    string(SUBSTRING "${body}" 0 ${body_length} body)
    set(${out_body} "${body}" PARENT_SCOPE)
    set(${out_rest} "${rest}" PARENT_SCOPE)
endfunction()

argx_take_block("${section}" "```c" code after_code)
argx_take_block("${after_code}" "```" transcript after_transcript)

set(failed FALSE)

# The code: greet.c after its first comment and the empty line.
string(FIND "${source}" "*/\n\n" comment_end)
if(comment_end EQUAL -1)
    message(SEND_ERROR "check_readme.cmake: ${ARGX_SOURCE} does not start with a comment and an empty line")
    set(failed TRUE)
else()
    math(EXPR body_start "${comment_end} + 4")
    string(SUBSTRING "${source}" ${body_start} -1 body)
    if(NOT body STREQUAL code)
        message(SEND_ERROR "check_readme.cmake: the code of the README is not the code of ${ARGX_SOURCE}")
        set(failed TRUE)
    endif()
endif()

# The command lines and their output.
set(rest "${transcript}")
set(count 0)
while(NOT rest STREQUAL "")
    string(FIND "${rest}" "\n" line_end)
    string(SUBSTRING "${rest}" 0 ${line_end} command_line)
    if(NOT command_line MATCHES "^\\$ greet( |$)")
        message(FATAL_ERROR "check_readme.cmake: expected a line \"$ greet ...\", found \"${command_line}\"")
    endif()
    string(REGEX REPLACE "^\\$ greet ?" "" arguments "${command_line}")
    math(EXPR output_start "${line_end} + 1")
    string(SUBSTRING "${rest}" ${output_start} -1 rest)

    string(FIND "${rest}" "\n\n$ greet" next)
    if(next EQUAL -1)
        set(expected "${rest}")
        set(rest "")
    else()
        math(EXPR expected_length "${next} + 1")
        math(EXPR next_start "${next} + 2")
        string(SUBSTRING "${rest}" 0 ${expected_length} expected)
        string(SUBSTRING "${rest}" ${next_start} -1 rest)
    endif()

    separate_arguments(argument_list UNIX_COMMAND "${arguments}")
    execute_process(
        COMMAND "${ARGX_PROGRAM}" ${argument_list}
        OUTPUT_VARIABLE actual
        ERROR_VARIABLE actual
    )
    string(REPLACE "\r\n" "\n" actual "${actual}")
    if(NOT actual STREQUAL expected)
        message(SEND_ERROR "check_readme.cmake: \"${command_line}\" printed\n${actual}\nbut the README shows\n${expected}")
        set(failed TRUE)
    endif()
    math(EXPR count "${count} + 1")
endwhile()

if(count EQUAL 0)
    message(FATAL_ERROR "check_readme.cmake: the output block of the README has no command line")
endif()

if(failed)
    message(FATAL_ERROR "check_readme.cmake: the README and examples/greet do not match")
endif()

message(STATUS "The code of the README and the output of its ${count} command lines match examples/greet")
