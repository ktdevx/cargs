# ClineInstall.cmake - the install rules and the package files of CLine.
#
# Included by the top-level CMakeLists.txt, after the target cline exists,
# and only if the option CLINE_INSTALL is ON. Installs, following
# GNUInstallDirs:
#
#   what                                         where
#   static library, import library,              ${CMAKE_INSTALL_LIBDIR}
#   shared library (not Windows)
#   DLL                                          ${CMAKE_INSTALL_BINDIR}
#   public headers (all of include/cline/)      ${CMAKE_INSTALL_INCLUDEDIR}/cline
#   CLineConfig.cmake,                          ${CMAKE_INSTALL_LIBDIR}/cmake/CLine
#   CLineConfigVersion.cmake,
#   CLineTargets*.cmake
#   cline.pc                                    ${CMAKE_INSTALL_LIBDIR}/pkgconfig
#
# A consumer calls find_package(CLine [<version>]) and links to
# CLine::cline, or asks pkg-config for cline.
#
# The version of the package files comes from the top-level CMakeLists.txt,
# which reads include/cline/version.h. cline.pc and CLineConfig.cmake
# (CLine_VERSION_FULL, CLine_VERSION_PRERELEASE) hold the full version,
# with the pre-release identifiers: cline_version_full and
# cline_version_prerelease. CLineConfigVersion.cmake, which decides
# compatibility, holds the numeric part, PROJECT_VERSION, like the
# SOVERSION. The file names and these steps are the same for a static and a
# shared library.
#
# Only the target cline and the public headers are installed. The internal
# headers in src/, the tests and the targets that exist for the tests are not.
#
# The installed files find each other by relative paths, so the install tree
# can be moved. The exception is an absolute CMAKE_INSTALL_LIBDIR or
# CMAKE_INSTALL_INCLUDEDIR: cline.pc then holds that absolute path.

include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# The directories below belong to CLine, also when another project embeds it.
set(cline_source_dir "${CMAKE_CURRENT_LIST_DIR}/..")
set(cline_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/cline-package")

# ---------------------------------------------------------------------------
# The library and the public headers.
# ---------------------------------------------------------------------------

# INCLUDES DESTINATION gives the installed target its include directory. It
# is the counterpart of the BUILD_INTERFACE directory in src/CMakeLists.txt.
install(TARGETS cline
    EXPORT CLineTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(DIRECTORY "${cline_source_dir}/include/cline"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

# ---------------------------------------------------------------------------
# The package for find_package(CLine).
# ---------------------------------------------------------------------------

set(cline_install_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/CLine")

# The exported target is CLine::cline. A shared library passes CLINE_SHARED
# on to its consumers; a static library passes no definition on.
install(EXPORT CLineTargets
    NAMESPACE CLine::
    FILE CLineTargets.cmake
    DESTINATION "${cline_install_cmakedir}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/CLineConfig.cmake.in"
    "${cline_generated_dir}/CLineConfig.cmake"
    INSTALL_DESTINATION "${cline_install_cmakedir}"
    NO_SET_AND_CHECK_MACRO
)

# The same boundary as the SOVERSION: while the major version is 0, a new
# minor version may break compatibility.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(cline_compatibility SameMinorVersion)
else()
    set(cline_compatibility SameMajorVersion)
endif()

write_basic_package_version_file(
    "${cline_generated_dir}/CLineConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY ${cline_compatibility}
)

install(FILES
    "${cline_generated_dir}/CLineConfig.cmake"
    "${cline_generated_dir}/CLineConfigVersion.cmake"
    DESTINATION "${cline_install_cmakedir}"
)

# ---------------------------------------------------------------------------
# The file for pkg-config: cline.pc.
# ---------------------------------------------------------------------------

# pkg-config sets ${pcfiledir} to the directory of the .pc file. The prefix
# is written relative to it, so the install tree can be moved. The made-up
# root only serves to count the directory levels between the two.
set(cline_pc_installdir "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    # A library directory outside the prefix has no relative path to it:
    # the paths are written as they are, and the install tree cannot be moved.
    set(cline_pc_prefix "${CMAKE_INSTALL_PREFIX}")
    set(cline_pc_libdir "${CMAKE_INSTALL_LIBDIR}")
else()
    file(RELATIVE_PATH cline_pc_prefix_from_pcfiledir
        "/cline-prefix/${cline_pc_installdir}"
        "/cline-prefix"
    )
    string(REGEX REPLACE "/+$" "" cline_pc_prefix_from_pcfiledir
        "${cline_pc_prefix_from_pcfiledir}")
    set(cline_pc_prefix "\${pcfiledir}/${cline_pc_prefix_from_pcfiledir}")
    set(cline_pc_libdir "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(cline_pc_includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(cline_pc_includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

# The counterpart of the PUBLIC definition in src/CMakeLists.txt: a shared
# library passes CLINE_SHARED on to its consumers.
get_target_property(cline_pc_target_type cline TYPE)
if(cline_pc_target_type STREQUAL "SHARED_LIBRARY")
    set(cline_pc_cflags " -DCLINE_SHARED")
else()
    set(cline_pc_cflags "")
endif()

# The name to link to is the name of the library that this configuration
# installs: the Debug library has a postfix. So there is one cline.pc for
# each configuration, and the install step takes the one of its
# configuration. The version is the full version, cline_version_full, with
# the pre-release identifiers.
set(cline_pc_library "$<TARGET_LINKER_FILE_BASE_NAME:cline>")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/cline.pc.in"
    "${cline_generated_dir}/cline.pc.in"
    @ONLY
)

file(GENERATE
    OUTPUT "${cline_generated_dir}/$<CONFIG>/cline.pc"
    INPUT "${cline_generated_dir}/cline.pc.in"
)

# The files of all configurations are written in the same instant, and the
# install step leaves a file alone that is as old as the one it would copy.
# Remove the installed file first, so that an install tree that gets more
# than one configuration always holds the cline.pc of the last one.
if(IS_ABSOLUTE "${cline_pc_installdir}")
    set(cline_pc_installed "\$ENV{DESTDIR}${cline_pc_installdir}/cline.pc")
else()
    set(cline_pc_installed
        "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${cline_pc_installdir}/cline.pc")
endif()
install(CODE "file(REMOVE \"${cline_pc_installed}\")")

install(FILES "${cline_generated_dir}/$<CONFIG>/cline.pc"
    DESTINATION "${cline_pc_installdir}"
)
