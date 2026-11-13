# The tests. Each call registers one test executable with CTest; see
# cmake/ClargsTesting.cmake.
#
# To add a test of the public API, put the file in public/ and add
#     clargs_add_test(<name> SOURCES public/<file>)
# To add a test of the internal functions, put the file in internal/ and add
#     clargs_add_test(<name> SOURCES internal/<file> INTERNAL)

clargs_add_test(alloc SOURCES public/alloc_test.c)
clargs_add_test(alloc_internal SOURCES internal/alloc_internal_test.c support/test_allocator.c INTERNAL)
clargs_add_test(command SOURCES public/command_test.c support/test_allocator.c)
clargs_add_test(error SOURCES public/error_test.c)
clargs_add_test(help SOURCES public/help_test.c support/test_allocator.c)
clargs_add_test(help_failure SOURCES public/help_failure_test.c support/test_allocator.c)
clargs_add_test(help_item SOURCES public/help_item_test.c support/test_allocator.c)
clargs_add_test(help_layout SOURCES public/help_layout_test.c support/test_allocator.c)
clargs_add_test(help_output SOURCES public/help_output_test.c support/test_allocator.c)
clargs_add_test(help_usage SOURCES public/help_usage_test.c support/test_allocator.c)
clargs_add_test(lex SOURCES internal/lex_test.c INTERNAL)
clargs_add_test(message SOURCES internal/message_test.c support/test_allocator.c INTERNAL)
clargs_add_test(name SOURCES internal/name_test.c INTERNAL)
clargs_add_test(option SOURCES public/option_test.c support/test_allocator.c)
clargs_add_test(out SOURCES internal/out_test.c INTERNAL)
clargs_add_test(parse_error SOURCES public/parse_error_test.c support/test_allocator.c)
clargs_add_test(parse_failure SOURCES public/parse_failure_test.c support/test_allocator.c)
clargs_add_test(parse_option SOURCES public/parse_option_test.c support/test_allocator.c)
clargs_add_test(parse_positional SOURCES public/parse_positional_test.c support/test_allocator.c)
clargs_add_test(parse_value SOURCES public/parse_value_test.c support/test_allocator.c)
clargs_add_test(parse_wide SOURCES public/parse_wide_test.c support/test_allocator.c)
clargs_add_test(positional SOURCES public/positional_test.c support/test_allocator.c)
clargs_add_test(number SOURCES internal/number_test.c support/test_allocator.c INTERNAL)
clargs_add_test(result SOURCES internal/result_test.c support/test_allocator.c INTERNAL)
clargs_add_test(schema_failure SOURCES public/schema_failure_test.c support/test_allocator.c)
clargs_add_test(str SOURCES internal/str_test.c support/test_allocator.c INTERNAL)
clargs_add_test(test_allocator SOURCES public/test_allocator_test.c support/test_allocator.c)
clargs_add_test(type SOURCES public/type_test.c support/test_allocator.c)
clargs_add_test(utf8 SOURCES internal/utf8_test.c INTERNAL)
clargs_add_test(validate SOURCES public/validate_test.c support/test_allocator.c)
clargs_add_test(vec SOURCES internal/vec_test.c support/test_allocator.c INTERNAL)
clargs_add_test(version SOURCES public/version_test.c)
clargs_add_test(version_internal SOURCES internal/version_internal_test.c INTERNAL)
clargs_add_test(wrap SOURCES internal/wrap_test.c INTERNAL)

# The tests of the release scripts and modules: tests/release/<name>_test.cmake,
# run with "cmake -P" (see tests/release/ReleaseTestHelpers.cmake). They need
# no build, so they are registered for a static and for a shared library alike,
# under the name release.<name> and with the label "release". Each test owns
# the work directory release/<name> of this build directory.
#
# To add a test, put the file in release/ and add
#     clargs_add_release_test(<name>)
function(clargs_add_release_test name)
    set(script "${CMAKE_CURRENT_SOURCE_DIR}/release/${name}_test.cmake")
    if(NOT EXISTS "${script}")
        message(FATAL_ERROR "clargs_add_release_test: there is no test ${script}")
    endif()
    add_test(NAME release.${name}
        COMMAND "${CMAKE_COMMAND}"
            "-DCLARGS_SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
            "-DCLARGS_TEST_WORK_DIR=${CMAKE_CURRENT_BINARY_DIR}/release/${name}"
            -P "${script}"
    )
    set_tests_properties(release.${name} PROPERTIES LABELS release)
endfunction()

clargs_add_release_test(version_read)
clargs_add_release_test(semver)
clargs_add_release_test(changelog)
clargs_add_release_test(version_header)

# The check of the allocations; see cmake/ClargsCheckAllocations.cmake. The
# library sources must not use the allocation functions of the C standard
# library directly. The test reads the sources and needs no build, so it is
# registered for a static and for a shared library alike.
add_test(NAME clargs.allocations
    COMMAND "${CMAKE_COMMAND}"
        "-DCLARGS_SOURCE_DIR=${PROJECT_SOURCE_DIR}/src"
        -P "${PROJECT_SOURCE_DIR}/cmake/ClargsCheckAllocations.cmake"
)

# The check of the exported symbols; see cmake/ClargsCheckExports.cmake. Only
# a shared library has exports to check. The test runs the script on the
# library of the configuration under test, with the tool of the toolchain.
get_target_property(clargs_type clargs TYPE)
if(clargs_type STREQUAL "SHARED_LIBRARY")
    if(WIN32)
        # dumpbin is next to the compiler and the linker. It is not on the
        # PATH outside a developer prompt.
        set(clargs_exports_format PE)
        get_filename_component(clargs_compiler_dir "${CMAKE_C_COMPILER}" DIRECTORY)
        get_filename_component(clargs_linker_dir "${CMAKE_LINKER}" DIRECTORY)
        find_program(clargs_exports_tool
            NAMES dumpbin
            HINTS "${clargs_linker_dir}" "${clargs_compiler_dir}"
            NO_CACHE
        )
    elseif(APPLE)
        set(clargs_exports_format MACHO)
        set(clargs_exports_tool "${CMAKE_NM}")
    else()
        set(clargs_exports_format ELF)
        set(clargs_exports_tool "${CMAKE_NM}")
    endif()
    if(NOT clargs_exports_tool)
        # The script looks on the PATH, and fails if the tool is not there.
        set(clargs_exports_tool "")
    endif()

    add_test(NAME clargs.exports
        COMMAND "${CMAKE_COMMAND}"
            "-DCLARGS_LIBRARY=$<TARGET_FILE:clargs>"
            "-DCLARGS_INCLUDE_DIR=${PROJECT_SOURCE_DIR}/include/clargs"
            "-DCLARGS_FORMAT=${clargs_exports_format}"
            "-DCLARGS_TOOL=${clargs_exports_tool}"
            -P "${PROJECT_SOURCE_DIR}/cmake/ClargsCheckExports.cmake"
    )
endif()
