# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# The complete example programs that the documents link to. Each one is built
# and run as a CTest test (label "example"), so that an example that no longer
# compiles or prints something else makes the tests fail. Added only when
# ARGX_BUILD_TESTS is ON.
#
# To add an example, put the file here and add
#     argx_add_example(<name> <expected output regex>)

function(argx_add_example name pass_regex)
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

    add_test(NAME "example.${name}" COMMAND "${target}")
    set_tests_properties("example.${name}" PROPERTIES
        PASS_REGULAR_EXPRESSION "${pass_regex}"
        LABELS example
    )
endfunction()

argx_add_example(schema_pack "^the schema of pack is valid\r?\n$")
