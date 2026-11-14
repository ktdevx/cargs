# SPDX-FileCopyrightText: 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# ArgXTesting.cmake - the test framework (Naos) and the registration of tests.
#
# Included only when the option ARGX_BUILD_TESTS is ON. Including it fetches
# Naos at configure time, so a build without tests never includes it.
#
#   argx_add_test(<name>
#       SOURCES <source>...
#       [INTERNAL])
#
# Registers one test executable. Call it from tests/CMakeLists.txt:
#
#   argx_add_test(version SOURCES public/version_test.c)
#   argx_add_test(alloc_internal SOURCES internal/alloc_internal_test.c INTERNAL)
#
#   - Makes the executable argx_test_<name> from the sources.
#   - Gives it the compile settings of the ArgX targets, without the strict
#     warnings of the libraries: argx_target_defaults(<target>).
#   - Links it to Naos::naos and to the code under test:
#       without INTERNAL  ArgX::argx, static or shared as configured, for
#                         the tests of the public API;
#       with INTERNAL     argx_internal, always linked statically, for the
#                         tests of the internal functions. The test includes
#                         an internal header as "alloc_internal.h".
#   - On Windows with a shared library, copies the DLLs a test of the public
#     API needs next to its executable, so that it starts with no change to
#     PATH.
#   - Registers every Naos test of the executable with CTest, under the name
#     <name>.<suite>.<test> and with the label "public" or "internal".
#
# <name> is made of letters, digits and underscores, and is used only once.
# The sources of one executable hold NAOS_MAIN exactly once, and every test
# runs at least one assertion (Naos fails a test that runs none).
#
# Stops with a FATAL_ERROR that names the mistake when the arguments are wrong.

include_guard(GLOBAL)

include(FetchContent)

# Naos, fixed to a release tag. This is the only place that names the tag.
# The Naos project keeps to itself inside another project: it builds a static
# library, and adds no tests and no install rules of its own.
FetchContent_Declare(Naos
    GIT_REPOSITORY https://github.com/vabelorg/naos.git
    GIT_TAG v0.4.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Naos)

function(argx_add_test)
    set(usage "Usage: argx_add_test(<name> SOURCES <source>... [INTERNAL]).")

    if(ARGC EQUAL 0)
        message(FATAL_ERROR "argx_add_test: the test name is missing. ${usage}")
    endif()
    set(name "${ARGV0}")
    if(name STREQUAL "SOURCES" OR name STREQUAL "INTERNAL")
        message(FATAL_ERROR
            "argx_add_test: the test name is missing: the first argument is "
            "the keyword ${name}. ${usage}")
    endif()
    if(NOT name MATCHES "^[A-Za-z0-9_]+$")
        message(FATAL_ERROR
            "argx_add_test: invalid test name \"${name}\": use letters, digits "
            "and underscores only. ${usage}")
    endif()

    cmake_parse_arguments(PARSE_ARGV 1 arg "INTERNAL" "" "SOURCES")
    if(DEFINED arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "argx_add_test(${name}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}. "
            "${usage}")
    endif()
    if(NOT DEFINED arg_SOURCES)
        message(FATAL_ERROR
            "argx_add_test(${name}): no source files: SOURCES is required and "
            "takes at least one file. ${usage}")
    endif()

    set(target "argx_test_${name}")
    if(TARGET "${target}")
        message(FATAL_ERROR
            "argx_add_test(${name}): the test name \"${name}\" is already used "
            "(the target ${target} exists).")
    endif()

    if(arg_INTERNAL)
        set(library argx_internal)
        set(label internal)
    else()
        set(library ArgX::argx)
        set(label public)
    endif()
    if(NOT TARGET "${library}")
        message(FATAL_ERROR
            "argx_add_test(${name}): the target ${library} does not exist. "
            "Tests are registered only when ARGX_BUILD_TESTS is ON.")
    endif()

    add_executable("${target}" ${arg_SOURCES})
    argx_target_defaults("${target}")
    target_link_libraries("${target}" PRIVATE Naos::naos "${library}")

    # Windows looks for a DLL next to the executable. This command is added
    # before the registration below on purpose: the commands that follow a
    # build run in the order they were added, and Naos lists the tests by
    # running the executable, which needs the DLL already.
    get_target_property(argx_type argx TYPE)
    if(WIN32 AND argx_type STREQUAL "SHARED_LIBRARY" AND NOT arg_INTERNAL)
        add_custom_command(TARGET "${target}" POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "$<TARGET_RUNTIME_DLLS:${target}>"
                "$<TARGET_FILE_DIR:${target}>"
            COMMAND_EXPAND_LISTS
            COMMENT "Copying the DLLs of ${target} next to it"
            VERBATIM
        )
    endif()

    naos_discover_tests("${target}"
        PREFIX "${name}."
        LABELS "${label}"
    )
endfunction()
