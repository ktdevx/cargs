# The consumer check: a small project that uses CLine the way a user does.
#
# It is not part of the CLine build. Copy this directory out of the
# repository and configure it as a project of its own:
#
#   cmake -S <copy> -B <build> -DCLINE_CONSUME_MODE=<mode> ...
#   cmake --build <build>
#   ctest --test-dir <build>          (or run the executable cline_consumer)
#
# CLINE_CONSUME_MODE chooses how CLine is brought in:
#
#   mode              how                              also give
#   find_package      find_package(CLine <version>)   CMAKE_PREFIX_PATH=<install prefix>
#   pkg_config        pkg-config, module cline        CMAKE_PREFIX_PATH=<install prefix>
#                                                      (or PKG_CONFIG_PATH)
#   add_subdirectory  add_subdirectory(<checkout>)     CLINE_CONSUME_SOURCE_DIR=<checkout>
#   fetch_content     FetchContent on <checkout>,      CLINE_CONSUME_SOURCE_DIR=<checkout>
#                     without the network
#
# The executable returns 0 if the version of the linked library equals the
# version of the headers. Besides that:
#
#   - find_package: the installed version is found when it is asked for, and
#     a version with another minor (another major from 1.0 on) is not;
#   - pkg_config: the version that pkg-config reports equals the headers';
#   - find_package, pkg_config: the full version that the package reports,
#     "<major>.<minor>.<patch>[-<pre-release>]", equals the headers' too,
#     the pre-release identifiers included (CLINE_VERSION_PRERELEASE);
#   - add_subdirectory, fetch_content: CLine keeps the defaults of an
#     embedded project: no tests, no Naos, no install rules, no warnings as
#     errors.
#
# The steps are the same for a static and a shared CLine. For the last two
# modes, BUILD_SHARED_LIBS chooses between them.

cmake_minimum_required(VERSION 3.21)

project(CLineConsumer LANGUAGES C)

set(consumer_modes find_package pkg_config add_subdirectory fetch_content)
set(CLINE_CONSUME_MODE "" CACHE STRING
    "How to bring CLine in: find_package, pkg_config, add_subdirectory or fetch_content")
set_property(CACHE CLINE_CONSUME_MODE PROPERTY STRINGS ${consumer_modes})
set(CLINE_CONSUME_SOURCE_DIR "" CACHE PATH
    "The CLine checkout, for the modes add_subdirectory and fetch_content")

if(NOT CLINE_CONSUME_MODE IN_LIST consumer_modes)
    list(JOIN consumer_modes ", " consumer_modes_text)
    message(FATAL_ERROR
        "CLINE_CONSUME_MODE is \"${CLINE_CONSUME_MODE}\". "
        "Set it to one of: ${consumer_modes_text}.")
endif()

# Checks that the embedded CLine in <source-dir> kept its defaults. Stops
# with a FATAL_ERROR that names everything it found otherwise.
function(consumer_check_embedded source_dir)
    set(problems "")

    foreach(option IN ITEMS CLINE_BUILD_TESTS CLINE_INSTALL CLINE_WARNINGS_AS_ERRORS)
        if(NOT DEFINED CACHE{${option}})
            list(APPEND problems "the option ${option} does not exist")
        elseif(${option})
            list(APPEND problems "the option ${option} is ON")
        endif()
    endforeach()

    if(NOT TARGET CLine::cline)
        list(APPEND problems "the target CLine::cline does not exist")
    endif()
    foreach(target IN ITEMS Naos::naos naos cline_internal)
        if(TARGET "${target}")
            list(APPEND problems "the target ${target} exists")
        endif()
    endforeach()

    # The directories of CLine: its own, and all it added below.
    set(directories "")
    set(pending "${source_dir}")
    while(pending)
        list(POP_FRONT pending directory)
        list(APPEND directories "${directory}")
        get_property(subdirectories DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
        list(APPEND pending ${subdirectories})
    endwhile()

    foreach(directory IN LISTS directories)
        # Tests come from add_test(), or from a file that ctest reads (the
        # tests that Naos lists after the build).
        get_property(tests DIRECTORY "${directory}" PROPERTY TESTS)
        get_property(test_files DIRECTORY "${directory}" PROPERTY TEST_INCLUDE_FILES)
        list(APPEND tests ${test_files})
        if(tests)
            list(JOIN tests ", " tests)
            list(APPEND problems "${directory} registers tests: ${tests}")
        endif()
        get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        list(REMOVE_ITEM targets cline)
        if(targets)
            list(JOIN targets ", " targets)
            list(APPEND problems "${directory} adds targets besides cline: ${targets}")
        endif()
    endforeach()

    # Every install rule belongs to a component, and this project has no
    # install rules of its own.
    get_cmake_property(components COMPONENTS)
    if(components)
        list(JOIN components ", " components)
        list(APPEND problems "install rules were added (components: ${components})")
    endif()

    if(TARGET cline)
        get_target_property(options cline COMPILE_OPTIONS)
        if(options MATCHES "(^|;)(-Werror|/WX)(;|$)")
            list(JOIN options " " options)
            list(APPEND problems "cline is compiled with warnings as errors: ${options}")
        endif()
    endif()

    if(problems)
        list(JOIN problems "\n  - " problems_text)
        message(FATAL_ERROR
            "CLine, embedded with ${CLINE_CONSUME_MODE}, did not keep the "
            "defaults of an embedded project:\n  - ${problems_text}")
    endif()
endfunction()

if(CLINE_CONSUME_MODE MATCHES "^(add_subdirectory|fetch_content)$"
        AND NOT EXISTS "${CLINE_CONSUME_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "CLINE_CONSUME_SOURCE_DIR is \"${CLINE_CONSUME_SOURCE_DIR}\". "
        "Set it to the CLine checkout for the mode ${CLINE_CONSUME_MODE}.")
endif()

set(consumer_library CLine::cline)
set(consumer_package_version "")

if(CLINE_CONSUME_MODE STREQUAL "find_package")
    # Without a version: tells which version is installed.
    find_package(CLine CONFIG REQUIRED)
    set(consumer_package_version "${CLine_VERSION}")
    set(consumer_package_dir "${CLine_DIR}")
    if(NOT consumer_package_version MATCHES "^([0-9]+)\\.([0-9]+)\\.[0-9]+$")
        message(FATAL_ERROR
            "find_package(CLine) reports the version \"${consumer_package_version}\", "
            "which is not <major>.<minor>.<patch>.")
    endif()
    set(consumer_major "${CMAKE_MATCH_1}")
    set(consumer_minor "${CMAKE_MATCH_2}")

    # The versions that the installed one is not compatible with. While the
    # major version is 0, another minor version; from 1.0 on, another major
    # version. A newer version than the installed one never is.
    math(EXPR consumer_next_minor "${consumer_minor} + 1")
    set(consumer_incompatible "${consumer_major}.${consumer_next_minor}")
    if(consumer_major EQUAL 0)
        if(consumer_minor GREATER 0)
            math(EXPR consumer_previous_minor "${consumer_minor} - 1")
            list(APPEND consumer_incompatible "0.${consumer_previous_minor}")
        endif()
    else()
        math(EXPR consumer_next_major "${consumer_major} + 1")
        math(EXPR consumer_previous_major "${consumer_major} - 1")
        list(APPEND consumer_incompatible
            "${consumer_next_major}.0" "${consumer_previous_major}.0")
    endif()
    foreach(version IN LISTS consumer_incompatible)
        find_package(CLine ${version} CONFIG QUIET)
        if(CLine_FOUND)
            message(FATAL_ERROR
                "find_package(CLine ${version}) found CLine "
                "${consumer_package_version} in ${CLine_DIR}. "
                "It must report that version as not found.")
        endif()
        message(STATUS "find_package(CLine ${version}): not found, as expected")
    endforeach()

    # The failed searches forgot where the package is.
    set(CLine_DIR "${consumer_package_dir}" CACHE PATH
        "The directory containing a CMake configuration file for CLine." FORCE)

    # What a user writes: ask for the version, link to CLine::cline.
    find_package(CLine ${consumer_package_version} CONFIG REQUIRED)

    # CLine_VERSION has only the numeric part, which the compatibility check
    # needs. The full version, with the pre-release identifiers, is in
    # CLine_VERSION_FULL. Its numeric part must be CLine_VERSION.
    if(NOT DEFINED CLine_VERSION_FULL)
        message(FATAL_ERROR
            "find_package(CLine) does not set CLine_VERSION_FULL, "
            "the full version of the package.")
    endif()
    if(NOT CLine_VERSION_FULL MATCHES "^([0-9]+\\.[0-9]+\\.[0-9]+)(-[0-9A-Za-z.-]+)?$"
            OR NOT CMAKE_MATCH_1 STREQUAL consumer_package_version)
        message(FATAL_ERROR
            "find_package(CLine) reports the full version \"${CLine_VERSION_FULL}\", "
            "which is not ${consumer_package_version}[-<pre-release>].")
    endif()
    set(consumer_package_version "${CLine_VERSION_FULL}")
elseif(CLINE_CONSUME_MODE STREQUAL "pkg_config")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CLINE REQUIRED IMPORTED_TARGET cline)
    set(consumer_library PkgConfig::CLINE)
    set(consumer_package_version "${CLINE_VERSION}")
elseif(CLINE_CONSUME_MODE STREQUAL "add_subdirectory")
    # The checkout is outside this project, so a build directory is named.
    add_subdirectory("${CLINE_CONSUME_SOURCE_DIR}" cline)
    consumer_check_embedded("${CLINE_CONSUME_SOURCE_DIR}")
else()
    # A user gives GIT_REPOSITORY and GIT_TAG here. The check points at the
    # checkout instead, so that it tests this commit and needs no network.
    include(FetchContent)
    FetchContent_Declare(CLine SOURCE_DIR "${CLINE_CONSUME_SOURCE_DIR}")
    FetchContent_MakeAvailable(CLine)
    consumer_check_embedded("${CLINE_CONSUME_SOURCE_DIR}")
endif()

add_executable(cline_consumer main.c)
target_link_libraries(cline_consumer PRIVATE ${consumer_library})

# The full version that the package reported, for main.c to compare with the
# headers: the numbers, and the pre-release identifiers as a string (empty for
# a release). The embedded modes have no package.
if(consumer_package_version)
    if(NOT consumer_package_version MATCHES
            "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-([0-9A-Za-z.-]+))?$")
        message(FATAL_ERROR
            "The package reports the version \"${consumer_package_version}\", "
            "which is not <major>.<minor>.<patch>[-<pre-release>].")
    endif()
    target_compile_definitions(cline_consumer PRIVATE
        "CONSUMER_PACKAGE_VERSION_MAJOR=${CMAKE_MATCH_1}"
        "CONSUMER_PACKAGE_VERSION_MINOR=${CMAKE_MATCH_2}"
        "CONSUMER_PACKAGE_VERSION_PATCH=${CMAKE_MATCH_3}"
        "CONSUMER_PACKAGE_VERSION_PRERELEASE=\"${CMAKE_MATCH_5}\""
    )
endif()

# Windows looks for a DLL next to the executable. Only a shared CLine has
# one: with no DLL the list is empty and the copy command would fail.
get_target_property(consumer_library_type ${consumer_library} TYPE)
if(WIN32 AND consumer_library_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET cline_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:cline_consumer>"
            "$<TARGET_FILE_DIR:cline_consumer>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying the DLLs of cline_consumer next to it"
        VERBATIM
    )
endif()

# So that ctest runs the executable, whatever the generator names its path.
enable_testing()
add_test(NAME cline_consumer COMMAND cline_consumer)

message(STATUS "CLine consumer: ${CLINE_CONSUME_MODE}, linking to ${consumer_library}")
