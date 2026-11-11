# ClargsInstall.cmake - the install rules and the package files of CLArgs.
#
# Included by the top-level CMakeLists.txt, after the target clargs exists,
# and only if the option CLARGS_INSTALL is ON. Installs, following
# GNUInstallDirs:
#
#   what                                         where
#   static library, import library,              ${CMAKE_INSTALL_LIBDIR}
#   shared library (not Windows)
#   DLL                                          ${CMAKE_INSTALL_BINDIR}
#   public headers (all of include/clargs/)      ${CMAKE_INSTALL_INCLUDEDIR}/clargs
#   CLArgsConfig.cmake,                          ${CMAKE_INSTALL_LIBDIR}/cmake/CLArgs
#   CLArgsConfigVersion.cmake,
#   CLArgsTargets*.cmake
#
# A consumer calls find_package(CLArgs [<version>]) and links to
# CLArgs::clargs. The file names and these steps are the same for a static
# and a shared library.
#
# Only the target clargs and the public headers are installed. The internal
# headers in src/, the tests and the targets that exist for the tests are not.
#
# The installed files find each other by relative paths, so the install tree
# can be moved.

include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# The directories below belong to CLArgs, also when another project embeds it.
set(clargs_source_dir "${CMAKE_CURRENT_LIST_DIR}/..")
set(clargs_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/clargs-package")

# ---------------------------------------------------------------------------
# The library and the public headers.
# ---------------------------------------------------------------------------

# INCLUDES DESTINATION gives the installed target its include directory. It
# is the counterpart of the BUILD_INTERFACE directory in src/CMakeLists.txt.
install(TARGETS clargs
    EXPORT CLArgsTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(DIRECTORY "${clargs_source_dir}/include/clargs"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

# ---------------------------------------------------------------------------
# The package for find_package(CLArgs).
# ---------------------------------------------------------------------------

set(clargs_install_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/CLArgs")

# The exported target is CLArgs::clargs. A shared library passes CLARGS_SHARED
# on to its consumers; a static library passes no definition on.
install(EXPORT CLArgsTargets
    NAMESPACE CLArgs::
    FILE CLArgsTargets.cmake
    DESTINATION "${clargs_install_cmakedir}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/CLArgsConfig.cmake.in"
    "${clargs_generated_dir}/CLArgsConfig.cmake"
    INSTALL_DESTINATION "${clargs_install_cmakedir}"
    NO_SET_AND_CHECK_MACRO
)

# The same boundary as the SOVERSION: while the major version is 0, a new
# minor version may break compatibility.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(clargs_compatibility SameMinorVersion)
else()
    set(clargs_compatibility SameMajorVersion)
endif()

write_basic_package_version_file(
    "${clargs_generated_dir}/CLArgsConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY ${clargs_compatibility}
)

install(FILES
    "${clargs_generated_dir}/CLArgsConfig.cmake"
    "${clargs_generated_dir}/CLArgsConfigVersion.cmake"
    DESTINATION "${clargs_install_cmakedir}"
)
