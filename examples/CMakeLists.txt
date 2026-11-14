# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# The complete example programs, which the README links to. Each one is built,
# and run as CTest tests (label "example") with the command lines that its
# comments show, so that an example that no longer compiles or prints
# something else makes the tests fail. Added only when ARGX_BUILD_TESTS is ON.
#
# To add an example, put <name>.c here, then add
#     argx_add_example(<name>)
# and one call per command line to run it with:
#     argx_add_example_test(<name> <case> <expected output regex> [ARGS <argument>...])
# The test is named example.<name>.<case>. Its standard output and standard
# error stream are matched together, and the exit status is not checked.

function(argx_add_example name)
    set(target "argx_example_${name}")
    add_executable("${target}" "${name}.c")
    argx_target_defaults("${target}")
    target_link_libraries("${target}" PRIVATE ArgX::argx)

    # Windows looks for a DLL next to the executable
    # (as in cmake/ArgXTesting.cmake).
    get_target_property(argx_type argx TYPE)
    if(WIN32 AND argx_type STREQUAL "SHARED_LIBRARY")
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:${target}>"
                "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS
            COMMENT "Copying the DLLs of ${target} next to it"
            VERBATIM
        )
    endif()
endfunction()

function(argx_add_example_test name case pass_regex)
    cmake_parse_arguments(PARSE_ARGV 3 example "" "" "ARGS")
    set(test "example.${name}.${case}")
    add_test(NAME "${test}" COMMAND "argx_example_${name}" ${example_ARGS})
    set_tests_properties("${test}" PROPERTIES
        PASS_REGULAR_EXPRESSION "${pass_regex}"
        LABELS example
    )
endfunction()

argx_add_example(pack)
argx_add_example_test(pack create
    "^verbose: 2, color: on\r?\ncreate out\\.tar, level 6 \\(default\\)\r?\nformat tar, block size 65536 bytes, ratio 0\\.50\r?\n  a\\.txt\r?\n  b\\.txt\r?\n$"
    ARGS -vv create -o out.tar a.txt b.txt)
argx_add_example_test(pack values
    "^verbose: 0, color: off\r?\ncreate out\\.zip, level 9 \\(command line\\)\r?\nformat zip, block size 2097152 bytes, ratio 0\\.50\r?\n  -x\\.txt\r?\n$"
    ARGS --no-color create -l9 --format=zip --block-size 2m -o out.zip -- -x.txt)
argx_add_example_test(pack list
    "^verbose: 0, color: on\r?\nlist out\\.tar, pattern \\* \\(default\\)\r?\n$"
    ARGS list out.tar)
argx_add_example_test(pack help
    "^Usage: pack create \\[OPTIONS\\] --output <FILE> <file>\\.\\.\\.\r?\n\r?\nCreate an archive\\.\r?\n"
    ARGS create --help)
argx_add_example_test(pack parse-error
    "^pack: missing required option '--output'\r?\nUsage: pack create \\[OPTIONS\\] --output <FILE> <file>\\.\\.\\.\r?\nTry 'pack --help' for more information\\.\r?\n$"
    ARGS create a.txt)
argx_add_example_test(pack no-command
    "^pack: a command is required\r?\nTry 'pack --help' for more information\\.\r?\n$")
