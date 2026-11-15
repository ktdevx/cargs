# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# The example "greet", the program of the README. Its tests come from the
# README itself: check_readme.cmake compares greet.c with the code of the
# README, and runs the command lines of the README and compares the output.

argx_add_example(greet)

add_test(NAME example.greet.readme
    COMMAND "${CMAKE_COMMAND}"
        "-DARGX_README=${PROJECT_SOURCE_DIR}/README.md"
        "-DARGX_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/greet.c"
        "-DARGX_PROGRAM=$<TARGET_FILE:argx_example_greet>"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/check_readme.cmake"
)
set_tests_properties(example.greet.readme PROPERTIES LABELS example)

# The command lines of the README that fail exit with a failure status.
argx_add_example_failure_test(greet missing-name)
argx_add_example_failure_test(greet invalid-value ARGS --count=many world)
