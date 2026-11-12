# The tests. Each call registers one test executable with CTest; see
# cmake/ClargsTesting.cmake.
#
# To add a test of the public API, put the file in public/ and add
#     clargs_add_test(<name> SOURCES public/<file>)
# To add a test of the internal functions, put the file in internal/ and add
#     clargs_add_test(<name> SOURCES internal/<file> INTERNAL)

clargs_add_test(alloc SOURCES public/alloc_test.c)
clargs_add_test(alloc_internal SOURCES internal/alloc_internal_test.c support/test_allocator.c INTERNAL)
clargs_add_test(error SOURCES public/error_test.c)
clargs_add_test(test_allocator SOURCES public/test_allocator_test.c support/test_allocator.c)
clargs_add_test(version SOURCES public/version_test.c)
clargs_add_test(version_internal SOURCES internal/version_internal_test.c INTERNAL)

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
