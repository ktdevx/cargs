cmake_minimum_required(VERSION 3.21)

# The public header include/argx/version.h is the single source of the
# version. Read it before project(), which takes the version as an argument.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArgXVersion.cmake")
argx_read_version("${CMAKE_CURRENT_LIST_DIR}/include/argx/version.h"
    argx_version argx_version_prerelease)

# The full version, "<major>.<minor>.<patch>[-<pre-release>]", for the package
# files. project() takes only the numeric part, so PROJECT_VERSION, the
# compatibility check of find_package and the SOVERSION stay numeric.
if(argx_version_prerelease STREQUAL "")
    set(argx_version_full "${argx_version}")
else()
    set(argx_version_full "${argx_version}-${argx_version_prerelease}")
endif()

project(ArgX VERSION ${argx_version} LANGUAGES C)

# Options. Each one is ON by default when ArgX is the top-level project and
# OFF when another project embeds it (add_subdirectory, FetchContent).
#
# Static or shared is chosen with the standard BUILD_SHARED_LIBS variable.
# This file sets no global state (compiler flags, C standard, output
# directories): every setting belongs to a ArgX target.
option(ARGX_BUILD_TESTS
    "Build the ArgX tests (fetches Naos) and register them with CTest"
    ${PROJECT_IS_TOP_LEVEL})
option(ARGX_INSTALL
    "Generate the ArgX install rules and package files"
    ${PROJECT_IS_TOP_LEVEL})
option(ARGX_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors in the ArgX targets"
    ${PROJECT_IS_TOP_LEVEL})

# The compile settings shared by the ArgX targets: argx_target_defaults().
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArgXTargetDefaults.cmake")

# The library.
add_subdirectory(src)

# The install rules and the package files.
if(ARGX_INSTALL)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArgXInstall.cmake")
endif()

# The tests. The testing module fetches Naos when it is included, so it is
# included only when the tests are built.
if(ARGX_BUILD_TESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ArgXTesting.cmake")
    enable_testing()
    add_subdirectory(tests)
endif()

# The configuration ctest uses when none is given. With a multi-config
# generator, "cmake --build" with no --config builds Debug, and ctest with no
# -C selects nothing: ctest reads this file first, which makes Debug its
# default. Only for the top-level project: a project that embeds ArgX keeps
# its own test settings.
get_property(argx_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(PROJECT_IS_TOP_LEVEL AND argx_is_multi_config)
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY TEST_INCLUDE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/cmake/ArgXCTestDefaults.cmake")
endif()
