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
#   clargs.pc                                    ${CMAKE_INSTALL_LIBDIR}/pkgconfig
#
# A consumer calls find_package(CLArgs [<version>]) and links to
# CLArgs::clargs, or asks pkg-config for clargs.
#
# The version of the package files comes from the top-level CMakeLists.txt,
# which reads include/clargs/version.h. clargs.pc and CLArgsConfig.cmake
# (CLArgs_VERSION_FULL, CLArgs_VERSION_PRERELEASE) hold the full version,
# with the pre-release identifiers: clargs_version_full and
# clargs_version_prerelease. CLArgsConfigVersion.cmake, which decides
# compatibility, holds the numeric part, PROJECT_VERSION, like the
# SOVERSION. The file names and these steps are the same for a static and a
# shared library.
#
# Only the target clargs and the public headers are installed. The internal
# headers in src/, the tests and the targets that exist for the tests are not.
#
# The installed files find each other by relative paths, so the install tree
# can be moved. The exception is an absolute CMAKE_INSTALL_LIBDIR or
# CMAKE_INSTALL_INCLUDEDIR: clargs.pc then holds that absolute path.

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

# ---------------------------------------------------------------------------
# The file for pkg-config: clargs.pc.
# ---------------------------------------------------------------------------

# pkg-config sets ${pcfiledir} to the directory of the .pc file. The prefix
# is written relative to it, so the install tree can be moved. The made-up
# root only serves to count the directory levels between the two.
set(clargs_pc_installdir "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    # A library directory outside the prefix has no relative path to it:
    # the paths are written as they are, and the install tree cannot be moved.
    set(clargs_pc_prefix "${CMAKE_INSTALL_PREFIX}")
    set(clargs_pc_libdir "${CMAKE_INSTALL_LIBDIR}")
else()
    file(RELATIVE_PATH clargs_pc_prefix_from_pcfiledir
        "/clargs-prefix/${clargs_pc_installdir}"
        "/clargs-prefix"
    )
    string(REGEX REPLACE "/+$" "" clargs_pc_prefix_from_pcfiledir
        "${clargs_pc_prefix_from_pcfiledir}")
    set(clargs_pc_prefix "\${pcfiledir}/${clargs_pc_prefix_from_pcfiledir}")
    set(clargs_pc_libdir "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(clargs_pc_includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(clargs_pc_includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

# The counterpart of the PUBLIC definition in src/CMakeLists.txt: a shared
# library passes CLARGS_SHARED on to its consumers.
get_target_property(clargs_pc_target_type clargs TYPE)
if(clargs_pc_target_type STREQUAL "SHARED_LIBRARY")
    set(clargs_pc_cflags " -DCLARGS_SHARED")
else()
    set(clargs_pc_cflags "")
endif()

# The name to link to is the name of the library that this configuration
# installs: the Debug library has a postfix. So there is one clargs.pc for
# each configuration, and the install step takes the one of its
# configuration. The version is the full version, clargs_version_full, with
# the pre-release identifiers.
set(clargs_pc_library "$<TARGET_LINKER_FILE_BASE_NAME:clargs>")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/clargs.pc.in"
    "${clargs_generated_dir}/clargs.pc.in"
    @ONLY
)

file(GENERATE
    OUTPUT "${clargs_generated_dir}/$<CONFIG>/clargs.pc"
    INPUT "${clargs_generated_dir}/clargs.pc.in"
)

# The files of all configurations are written in the same instant, and the
# install step leaves a file alone that is as old as the one it would copy.
# Remove the installed file first, so that an install tree that gets more
# than one configuration always holds the clargs.pc of the last one.
if(IS_ABSOLUTE "${clargs_pc_installdir}")
    set(clargs_pc_installed "\$ENV{DESTDIR}${clargs_pc_installdir}/clargs.pc")
else()
    set(clargs_pc_installed
        "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${clargs_pc_installdir}/clargs.pc")
endif()
install(CODE "file(REMOVE \"${clargs_pc_installed}\")")

install(FILES "${clargs_generated_dir}/$<CONFIG>/clargs.pc"
    DESTINATION "${clargs_pc_installdir}"
)
