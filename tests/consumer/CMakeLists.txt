# The consumer check: a small project that uses CLArgs the way a user does.
#
# It is not part of the CLArgs build. Copy this directory out of the
# repository and configure it as a project of its own:
#
#   cmake -S <copy> -B <build> -DCLARGS_CONSUME_MODE=<mode> ...
#   cmake --build <build>
#   ctest --test-dir <build>          (or run the executable clargs_consumer)
#
# CLARGS_CONSUME_MODE chooses how CLArgs is brought in:
#
#   mode              how                              also give
#   find_package      find_package(CLArgs <version>)   CMAKE_PREFIX_PATH=<install prefix>
#   pkg_config        pkg-config, module clargs        CMAKE_PREFIX_PATH=<install prefix>
#                                                      (or PKG_CONFIG_PATH)
#   add_subdirectory  add_subdirectory(<checkout>)     CLARGS_CONSUME_SOURCE_DIR=<checkout>
#   fetch_content     FetchContent on <checkout>,      CLARGS_CONSUME_SOURCE_DIR=<checkout>
#                     without the network
#
# The executable returns 0 if the version of the linked library equals the
# version of the headers. Besides that:
#
#   - find_package: the installed version is found when it is asked for, and
#     a version with another minor (another major from 1.0 on) is not;
#   - pkg_config: the version that pkg-config reports equals the headers';
#   - add_subdirectory, fetch_content: CLArgs keeps the defaults of an
#     embedded project: no tests, no Naos, no install rules, no warnings as
#     errors.
#
# The steps are the same for a static and a shared CLArgs. For the last two
# modes, BUILD_SHARED_LIBS chooses between them.

cmake_minimum_required(VERSION 3.21)

project(CLArgsConsumer LANGUAGES C)

set(consumer_modes find_package pkg_config add_subdirectory fetch_content)
set(CLARGS_CONSUME_MODE "" CACHE STRING
    "How to bring CLArgs in: find_package, pkg_config, add_subdirectory or fetch_content")
set_property(CACHE CLARGS_CONSUME_MODE PROPERTY STRINGS ${consumer_modes})
set(CLARGS_CONSUME_SOURCE_DIR "" CACHE PATH
    "The CLArgs checkout, for the modes add_subdirectory and fetch_content")

if(NOT CLARGS_CONSUME_MODE IN_LIST consumer_modes)
    list(JOIN consumer_modes ", " consumer_modes_text)
    message(FATAL_ERROR
        "CLARGS_CONSUME_MODE is \"${CLARGS_CONSUME_MODE}\". "
        "Set it to one of: ${consumer_modes_text}.")
endif()

# Checks that the embedded CLArgs in <source-dir> kept its defaults. Stops
# with a FATAL_ERROR that names everything it found otherwise.
function(consumer_check_embedded source_dir)
    set(problems "")

    foreach(option IN ITEMS CLARGS_BUILD_TESTS CLARGS_INSTALL CLARGS_WARNINGS_AS_ERRORS)
        if(NOT DEFINED CACHE{${option}})
            list(APPEND problems "the option ${option} does not exist")
        elseif(${option})
            list(APPEND problems "the option ${option} is ON")
        endif()
    endforeach()

    if(NOT TARGET CLArgs::clargs)
        list(APPEND problems "the target CLArgs::clargs does not exist")
    endif()
    foreach(target IN ITEMS Naos::naos naos clargs_internal)
        if(TARGET "${target}")
            list(APPEND problems "the target ${target} exists")
        endif()
    endforeach()

    # The directories of CLArgs: its own, and all it added below.
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
        list(REMOVE_ITEM targets clargs)
        if(targets)
            list(JOIN targets ", " targets)
            list(APPEND problems "${directory} adds targets besides clargs: ${targets}")
        endif()
    endforeach()

    # Every install rule belongs to a component, and this project has no
    # install rules of its own.
    get_cmake_property(components COMPONENTS)
    if(components)
        list(JOIN components ", " components)
        list(APPEND problems "install rules were added (components: ${components})")
    endif()

    if(TARGET clargs)
        get_target_property(options clargs COMPILE_OPTIONS)
        if(options MATCHES "(^|;)(-Werror|/WX)(;|$)")
            list(JOIN options " " options)
            list(APPEND problems "clargs is compiled with warnings as errors: ${options}")
        endif()
    endif()

    if(problems)
        list(JOIN problems "\n  - " problems_text)
        message(FATAL_ERROR
            "CLArgs, embedded with ${CLARGS_CONSUME_MODE}, did not keep the "
            "defaults of an embedded project:\n  - ${problems_text}")
    endif()
endfunction()

if(CLARGS_CONSUME_MODE MATCHES "^(add_subdirectory|fetch_content)$"
        AND NOT EXISTS "${CLARGS_CONSUME_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "CLARGS_CONSUME_SOURCE_DIR is \"${CLARGS_CONSUME_SOURCE_DIR}\". "
        "Set it to the CLArgs checkout for the mode ${CLARGS_CONSUME_MODE}.")
endif()

set(consumer_library CLArgs::clargs)
set(consumer_package_version "")

if(CLARGS_CONSUME_MODE STREQUAL "find_package")
    # Without a version: tells which version is installed.
    find_package(CLArgs CONFIG REQUIRED)
    set(consumer_package_version "${CLArgs_VERSION}")
    set(consumer_package_dir "${CLArgs_DIR}")
    if(NOT consumer_package_version MATCHES "^([0-9]+)\\.([0-9]+)\\.[0-9]+$")
        message(FATAL_ERROR
            "find_package(CLArgs) reports the version \"${consumer_package_version}\", "
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
        find_package(CLArgs ${version} CONFIG QUIET)
        if(CLArgs_FOUND)
            message(FATAL_ERROR
                "find_package(CLArgs ${version}) found CLArgs "
                "${consumer_package_version} in ${CLArgs_DIR}. "
                "It must report that version as not found.")
        endif()
        message(STATUS "find_package(CLArgs ${version}): not found, as expected")
    endforeach()

    # The failed searches forgot where the package is.
    set(CLArgs_DIR "${consumer_package_dir}" CACHE PATH
        "The directory containing a CMake configuration file for CLArgs." FORCE)

    # What a user writes: ask for the version, link to CLArgs::clargs.
    find_package(CLArgs ${consumer_package_version} CONFIG REQUIRED)
elseif(CLARGS_CONSUME_MODE STREQUAL "pkg_config")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CLARGS REQUIRED IMPORTED_TARGET clargs)
    set(consumer_library PkgConfig::CLARGS)
    set(consumer_package_version "${CLARGS_VERSION}")
elseif(CLARGS_CONSUME_MODE STREQUAL "add_subdirectory")
    # The checkout is outside this project, so a build directory is named.
    add_subdirectory("${CLARGS_CONSUME_SOURCE_DIR}" clargs)
    consumer_check_embedded("${CLARGS_CONSUME_SOURCE_DIR}")
else()
    # A user gives GIT_REPOSITORY and GIT_TAG here. The check points at the
    # checkout instead, so that it tests this commit and needs no network.
    include(FetchContent)
    FetchContent_Declare(CLArgs SOURCE_DIR "${CLARGS_CONSUME_SOURCE_DIR}")
    FetchContent_MakeAvailable(CLArgs)
    consumer_check_embedded("${CLARGS_CONSUME_SOURCE_DIR}")
endif()

add_executable(clargs_consumer main.c)
target_link_libraries(clargs_consumer PRIVATE ${consumer_library})

# The version that the package reported, for main.c to compare with the
# headers. The embedded modes have no package.
if(consumer_package_version)
    if(NOT consumer_package_version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)$")
        message(FATAL_ERROR
            "The package reports the version \"${consumer_package_version}\", "
            "which is not <major>.<minor>.<patch>.")
    endif()
    target_compile_definitions(clargs_consumer PRIVATE
        "CONSUMER_PACKAGE_VERSION_MAJOR=${CMAKE_MATCH_1}"
        "CONSUMER_PACKAGE_VERSION_MINOR=${CMAKE_MATCH_2}"
        "CONSUMER_PACKAGE_VERSION_PATCH=${CMAKE_MATCH_3}"
    )
endif()

# Windows looks for a DLL next to the executable. Only a shared CLArgs has
# one: with no DLL the list is empty and the copy command would fail.
get_target_property(consumer_library_type ${consumer_library} TYPE)
if(WIN32 AND consumer_library_type STREQUAL "SHARED_LIBRARY")
    add_custom_command(TARGET clargs_consumer POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_RUNTIME_DLLS:clargs_consumer>"
            "$<TARGET_FILE_DIR:clargs_consumer>"
        COMMAND_EXPAND_LISTS
        COMMENT "Copying the DLLs of clargs_consumer next to it"
        VERBATIM
    )
endif()

# So that ctest runs the executable, whatever the generator names its path.
enable_testing()
add_test(NAME clargs_consumer COMMAND clargs_consumer)

message(STATUS "CLArgs consumer: ${CLARGS_CONSUME_MODE}, linking to ${consumer_library}")
