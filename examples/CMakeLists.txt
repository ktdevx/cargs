# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# The complete example programs that the documents link to, one directory
# each. Each one is built, and run as CTest tests (label "example") with the
# command lines that its comments or the README show, so that an example that
# no longer compiles or prints something else makes the tests fail. Added
# only when ARGX_BUILD_TESTS is ON.
#
# To add an example, create the directory <name>/ with <name>.c and a
# CMakeLists.txt that calls
#     argx_add_example(<name>)
# and one call per command line to run it with:
#     argx_add_example_test(<name> <case> <expected output regex> [ARGS <argument>...])
# The test is named example.<name>.<case>. Its standard output and standard
# error stream are matched together, and the exit status is not checked. To
# check that a command line fails, add
#     argx_add_example_failure_test(<name> <case> [ARGS <argument>...])
# which passes when the program exits with a status other than 0. Then add
# the directory below.

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

function(argx_add_example_failure_test name case)
    cmake_parse_arguments(PARSE_ARGV 2 example "" "" "ARGS")
    set(test "example.${name}.${case}")
    add_test(NAME "${test}" COMMAND "argx_example_${name}" ${example_ARGS})
    set_tests_properties("${test}" PROPERTIES WILL_FAIL TRUE LABELS example)
endfunction()

add_subdirectory(greet)
add_subdirectory(pack)
