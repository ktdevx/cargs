cmake_minimum_required(VERSION 3.21)

# The public header include/clargs/version.h is the single source of the
# version. Read it before project(), which takes the version as an argument.
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClargsVersion.cmake")
clargs_read_version("${CMAKE_CURRENT_LIST_DIR}/include/clargs/version.h" clargs_version)

project(CLArgs VERSION ${clargs_version} LANGUAGES C)

# Options. Each one is ON by default when CLArgs is the top-level project and
# OFF when another project embeds it (add_subdirectory, FetchContent).
#
# Static or shared is chosen with the standard BUILD_SHARED_LIBS variable.
# This file sets no global state (compiler flags, C standard, output
# directories): every setting belongs to a CLArgs target.
option(CLARGS_BUILD_TESTS
    "Build the CLArgs tests (fetches Naos) and register them with CTest"
    ${PROJECT_IS_TOP_LEVEL})
option(CLARGS_INSTALL
    "Generate the CLArgs install rules and package files"
    ${PROJECT_IS_TOP_LEVEL})
option(CLARGS_WARNINGS_AS_ERRORS
    "Treat compiler warnings as errors in the CLArgs targets"
    ${PROJECT_IS_TOP_LEVEL})

# The compile settings shared by the CLArgs targets: clargs_target_defaults().
include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClargsTargetDefaults.cmake")

# The library.
add_subdirectory(src)

# The install rules and the package files.
if(CLARGS_INSTALL)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClargsInstall.cmake")
endif()

# The tests. The testing module fetches Naos when it is included, so it is
# included only when the tests are built.
if(CLARGS_BUILD_TESTS)
    include("${CMAKE_CURRENT_LIST_DIR}/cmake/ClargsTesting.cmake")
    enable_testing()
    add_subdirectory(tests)
endif()

# The configuration ctest uses when none is given. With a multi-config
# generator, "cmake --build" with no --config builds Debug, and ctest with no
# -C selects nothing: ctest reads this file first, which makes Debug its
# default. Only for the top-level project: a project that embeds CLArgs keeps
# its own test settings.
get_property(clargs_is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(PROJECT_IS_TOP_LEVEL AND clargs_is_multi_config)
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY TEST_INCLUDE_FILES
        "${CMAKE_CURRENT_LIST_DIR}/cmake/ClargsCTestDefaults.cmake")
endif()
