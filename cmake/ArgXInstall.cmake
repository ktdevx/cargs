# ArgXInstall.cmake - the install rules and the package files of ArgX.
#
# Included by the top-level CMakeLists.txt, after the target argx exists,
# and only if the option ARGX_INSTALL is ON. Installs, following
# GNUInstallDirs:
#
#   what                                         where
#   static library, import library,              ${CMAKE_INSTALL_LIBDIR}
#   shared library (not Windows)
#   DLL                                          ${CMAKE_INSTALL_BINDIR}
#   public headers (all of include/argx/)      ${CMAKE_INSTALL_INCLUDEDIR}/argx
#   ArgXConfig.cmake,                          ${CMAKE_INSTALL_LIBDIR}/cmake/ArgX
#   ArgXConfigVersion.cmake,
#   ArgXTargets*.cmake
#   argx.pc                                    ${CMAKE_INSTALL_LIBDIR}/pkgconfig
#
# A consumer calls find_package(ArgX [<version>]) and links to
# ArgX::argx, or asks pkg-config for argx.
#
# The version of the package files comes from the top-level CMakeLists.txt,
# which reads include/argx/version.h. argx.pc and ArgXConfig.cmake
# (ArgX_VERSION_FULL, ArgX_VERSION_PRERELEASE) hold the full version,
# with the pre-release identifiers: argx_version_full and
# argx_version_prerelease. ArgXConfigVersion.cmake, which decides
# compatibility, holds the numeric part, PROJECT_VERSION, like the
# SOVERSION. The file names and these steps are the same for a static and a
# shared library.
#
# Only the target argx and the public headers are installed. The internal
# headers in src/, the tests and the targets that exist for the tests are not.
#
# The installed files find each other by relative paths, so the install tree
# can be moved. The exception is an absolute CMAKE_INSTALL_LIBDIR or
# CMAKE_INSTALL_INCLUDEDIR: argx.pc then holds that absolute path.

include_guard(GLOBAL)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# The directories below belong to ArgX, also when another project embeds it.
set(argx_source_dir "${CMAKE_CURRENT_LIST_DIR}/..")
set(argx_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/argx-package")

# ---------------------------------------------------------------------------
# The library and the public headers.
# ---------------------------------------------------------------------------

# INCLUDES DESTINATION gives the installed target its include directory. It
# is the counterpart of the BUILD_INTERFACE directory in src/CMakeLists.txt.
install(TARGETS argx
    EXPORT ArgXTargets
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(DIRECTORY "${argx_source_dir}/include/argx"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    FILES_MATCHING PATTERN "*.h"
)

# ---------------------------------------------------------------------------
# The package for find_package(ArgX).
# ---------------------------------------------------------------------------

set(argx_install_cmakedir "${CMAKE_INSTALL_LIBDIR}/cmake/ArgX")

# The exported target is ArgX::argx. A shared library passes ARGX_SHARED
# on to its consumers; a static library passes no definition on.
install(EXPORT ArgXTargets
    NAMESPACE ArgX::
    FILE ArgXTargets.cmake
    DESTINATION "${argx_install_cmakedir}"
)

configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/ArgXConfig.cmake.in"
    "${argx_generated_dir}/ArgXConfig.cmake"
    INSTALL_DESTINATION "${argx_install_cmakedir}"
    NO_SET_AND_CHECK_MACRO
)

# The same boundary as the SOVERSION: while the major version is 0, a new
# minor version may break compatibility.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(argx_compatibility SameMinorVersion)
else()
    set(argx_compatibility SameMajorVersion)
endif()

write_basic_package_version_file(
    "${argx_generated_dir}/ArgXConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY ${argx_compatibility}
)

install(FILES
    "${argx_generated_dir}/ArgXConfig.cmake"
    "${argx_generated_dir}/ArgXConfigVersion.cmake"
    DESTINATION "${argx_install_cmakedir}"
)

# ---------------------------------------------------------------------------
# The file for pkg-config: argx.pc.
# ---------------------------------------------------------------------------

# pkg-config sets ${pcfiledir} to the directory of the .pc file. The prefix
# is written relative to it, so the install tree can be moved. The made-up
# root only serves to count the directory levels between the two.
set(argx_pc_installdir "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

if(IS_ABSOLUTE "${CMAKE_INSTALL_LIBDIR}")
    # A library directory outside the prefix has no relative path to it:
    # the paths are written as they are, and the install tree cannot be moved.
    set(argx_pc_prefix "${CMAKE_INSTALL_PREFIX}")
    set(argx_pc_libdir "${CMAKE_INSTALL_LIBDIR}")
else()
    file(RELATIVE_PATH argx_pc_prefix_from_pcfiledir
        "/argx-prefix/${argx_pc_installdir}"
        "/argx-prefix"
    )
    string(REGEX REPLACE "/+$" "" argx_pc_prefix_from_pcfiledir
        "${argx_pc_prefix_from_pcfiledir}")
    set(argx_pc_prefix "\${pcfiledir}/${argx_pc_prefix_from_pcfiledir}")
    set(argx_pc_libdir "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
endif()

if(IS_ABSOLUTE "${CMAKE_INSTALL_INCLUDEDIR}")
    set(argx_pc_includedir "${CMAKE_INSTALL_INCLUDEDIR}")
else()
    set(argx_pc_includedir "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
endif()

# The counterpart of the PUBLIC definition in src/CMakeLists.txt: a shared
# library passes ARGX_SHARED on to its consumers.
get_target_property(argx_pc_target_type argx TYPE)
if(argx_pc_target_type STREQUAL "SHARED_LIBRARY")
    set(argx_pc_cflags " -DARGX_SHARED")
else()
    set(argx_pc_cflags "")
endif()

# The name to link to is the name of the library that this configuration
# installs: the Debug library has a postfix. So there is one argx.pc for
# each configuration, and the install step takes the one of its
# configuration. The version is the full version, argx_version_full, with
# the pre-release identifiers.
set(argx_pc_library "$<TARGET_LINKER_FILE_BASE_NAME:argx>")

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/argx.pc.in"
    "${argx_generated_dir}/argx.pc.in"
    @ONLY
)

file(GENERATE
    OUTPUT "${argx_generated_dir}/$<CONFIG>/argx.pc"
    INPUT "${argx_generated_dir}/argx.pc.in"
)

# The files of all configurations are written in the same instant, and the
# install step leaves a file alone that is as old as the one it would copy.
# Remove the installed file first, so that an install tree that gets more
# than one configuration always holds the argx.pc of the last one.
if(IS_ABSOLUTE "${argx_pc_installdir}")
    set(argx_pc_installed "\$ENV{DESTDIR}${argx_pc_installdir}/argx.pc")
else()
    set(argx_pc_installed
        "\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${argx_pc_installdir}/argx.pc")
endif()
install(CODE "file(REMOVE \"${argx_pc_installed}\")")

install(FILES "${argx_generated_dir}/$<CONFIG>/argx.pc"
    DESTINATION "${argx_pc_installdir}"
)
