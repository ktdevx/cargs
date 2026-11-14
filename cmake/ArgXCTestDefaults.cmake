# ArgXCTestDefaults.cmake - the configuration ctest uses when none is given.
#
# This file is not included by a CMakeLists.txt. ctest reads it, before the
# tests of the build directory, each time it runs: the top CMakeLists.txt adds
# it to the directory property TEST_INCLUDE_FILES of the top directory, only
# when ArgX is the top-level project and the generator is multi-config
# (Visual Studio, Xcode, Ninja Multi-Config).
#
# With such a generator, "cmake --build <dir>" with no --config builds Debug,
# but "ctest --test-dir <dir>" with no -C selects no configuration and finds
# no test to run. This file makes Debug the default, so that the two commands
# agree. A configuration given to ctest (-C, --build-config) is kept.

if("${CTEST_CONFIGURATION_TYPE}" STREQUAL "")
    set(CTEST_CONFIGURATION_TYPE "Debug")
endif()
