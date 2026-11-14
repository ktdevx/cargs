cmake_minimum_required(VERSION 3.21)

# The public header include/cline/version.h is the single source of the
# version. Read it before project(), which takes the version as an argument.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClineVersion.cmake")
cline_read_version("${CMAKE_CURRENT_LIST_DIR}/include/cline/version.h"
    cline_version cline_version_prerelease)

# The full version, "<major>.<minor>.<patch>[-<pre-release>]", for the package
# files. project() takes only the numeric part, so PROJECT_VERSION, the
# compatibility check of find_package and the SOVERSION stay numeric.
if(cline_version_prerelease STREQUAL "")
    set(cline_version_full "${cline_version}")
else()
    set(cline_version_full "${cline_version}-${cline_version_prerelease}")
endif()

project(CLine VERSION ${cline_version} LANGUAGES C)

# Options. Each one is ON by default when CLine is the top-level project and
# OFF when another project embeds it (add_subdirectory, FetchContent).
#
# Static or shared is chosen with the standard BUILD_SHARED_LIBS variable.
# This file sets no global state (compiler flags, C standard, output
# directories): every setting belongs to a CLine target.
option(CLINE_BUILD_TESTS
    "Build the CLine tests (fetches Naos) and register them with CTest"
    ${PROJECT_IS_TOP_LEVEL})
option(CLINE_INSTALL
    "Generate the CLine install rules and package files"
    ${PROJECT_IS_TOP_LEVEL})
option(CLINE_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors in the CLine targets"
    ${PROJECT_IS_TOP_LEVEL})

# The compile settings shared by the CLine targets: cline_target_defaults().
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClineTargetDefaults.cmake")

# The library.
add_subdirectory(src)

# The install rules and the package files.
if(CLINE_INSTALL)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClineInstall.cmake")
endif()

# The tests. The testing module fetches Naos when it is included, so it is
# included only when the tests are built.
if(CLINE_BUILD_TESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClineTesting.cmake")
    enable_testing()
    add_subdirectory(tests)
endif()

# The configuration ctest uses when none is given. With a multi-config
# generator, "cmake --build" with no --config builds Debug, and ctest with no
# -C selects nothing: ctest reads this file first, which makes Debug its
# default. Only for the top-level project: a project that embeds CLine keeps
# its own test settings.
get_property(cline_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(PROJECT_IS_TOP_LEVEL AND cline_is_multi_config)
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY TEST_INCLUDE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/cmake/ClineCTestDefaults.cmake")
endif()
