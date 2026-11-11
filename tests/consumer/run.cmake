# run.cmake - runs the consumer check from start to end.
#
# A script, run with "cmake -P" on a build directory of CLArgs that is
# already built. CI calls it, and a developer runs the same command:
#
#   cmake -D CLARGS_BUILD_DIR=<dir> -D CLARGS_CONFIG=<Debug|Release>
#         -D CLARGS_SHARED=<ON|OFF> -P tests/consumer/run.cmake
#
#   CLARGS_BUILD_DIR  The build directory of CLArgs, configured with
#                     CLARGS_INSTALL=ON and built. Required.
#   CLARGS_CONFIG     The configuration to install and to build the consumer
#                     with. Required. It must be a configuration of the build
#                     directory: its CMAKE_BUILD_TYPE, or one of its
#                     CMAKE_CONFIGURATION_TYPES.
#   CLARGS_SHARED     ON if the build directory builds a shared library, OFF
#                     if a static one. Required. It must agree with the build
#                     directory; the embedded modes build the same shape.
#
# The steps:
#
#   1. Install the build directory into a temporary place.
#   2. Rename the install directory. From here on only the moved place is
#      used, so the installed files must find each other by relative paths.
#   3. Copy tests/consumer/ to a work directory and, as a project of its own,
#      configure, build and run it once for each mode, in this order:
#      find_package, pkg_config, add_subdirectory, fetch_content.
#
# Everything is written below <CLARGS_BUILD_DIR>/consumer-check, which is
# removed and made again on every run. Nothing is written to the source tree.
#
# The consumer is built with the generator and the compiler of the build
# directory. The first two modes use the moved install tree, the last two
# this checkout (no network).
#
# The mode pkg_config runs on Linux and macOS. On Windows the script only
# checks that clargs.pc is installed, and reports the mode as skipped.
#
# Every mode runs, also after another one failed. The script ends with 0 if
# all of them passed. Otherwise it prints the command that failed with its
# output, and ends with a FATAL_ERROR that names the modes that failed.
# A wrong argument and a failed install end the script at once.

cmake_minimum_required(VERSION 3.21)

set(clargs_consumer_usage
    "Usage:\n"
    "  cmake -D CLARGS_BUILD_DIR=<dir> -D CLARGS_CONFIG=<Debug|Release> "
    "-D CLARGS_SHARED=<ON|OFF> -P ${CMAKE_CURRENT_LIST_FILE}")
list(JOIN clargs_consumer_usage "" clargs_consumer_usage)

# clargs_consumer_run(<result-var> <text-var> <command> [<argument>...])
#
# Runs the command. Sets <result-var> to its exit code (or to the reason why
# it did not run), and <text-var> to the command line followed by all that
# the command printed.
function(clargs_consumer_run result_var text_var)
    execute_process(
        COMMAND ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE output
    )
    set(command_line "")
    foreach(argument IN LISTS ARGN)
        if(argument MATCHES "[ \t]")
            string(APPEND command_line " \"${argument}\"")
        else()
            string(APPEND command_line " ${argument}")
        endif()
    endforeach()
    set(${result_var} "${result}" PARENT_SCOPE)
    set(${text_var} "The command\n ${command_line}\nended with ${result}:\n${output}" PARENT_SCOPE)
endfunction()

# clargs_consumer_check_mode(<mode> [<configure-argument>...])
#
# Configures, builds and runs the copy of the consumer project for <mode>.
# Appends a line for the mode to clargs_consumer_results and, if a step
# failed, the mode and the step to clargs_consumer_failed.
function(clargs_consumer_check_mode mode)
    set(build_dir "${clargs_consumer_work_dir}/build-${mode}")
    message(STATUS "consumer check: ${mode} ...")

    set(failed_step "")
    foreach(step IN ITEMS configure build run)
        if(step STREQUAL "configure")
            clargs_consumer_run(result text
                "${CMAKE_COMMAND}"
                -S "${clargs_consumer_project_dir}"
                -B "${build_dir}"
                ${clargs_consumer_toolchain_arguments}
                "-DCLARGS_CONSUME_MODE=${mode}"
                ${ARGN}
            )
        elseif(step STREQUAL "build")
            clargs_consumer_run(result text
                "${CMAKE_COMMAND}" --build "${build_dir}" --config "${CLARGS_CONFIG}"
            )
        else()
            # The consumer registers its executable with CTest, so its path
            # does not matter. No test at all is a failure, not a success.
            clargs_consumer_run(result text
                "${CMAKE_CTEST_COMMAND}" --test-dir "${build_dir}" -C "${CLARGS_CONFIG}"
                --output-on-failure --no-tests=error
            )
        endif()
        if(NOT result STREQUAL "0")
            set(failed_step "${step}")
            break()
        endif()
    endforeach()

    if(failed_step STREQUAL "")
        message(STATUS "consumer check: ${mode} passed")
        list(APPEND clargs_consumer_results "  ${mode}: passed")
    else()
        message(STATUS "consumer check: ${mode} FAILED in the step \"${failed_step}\".\n${text}")
        list(APPEND clargs_consumer_results "  ${mode}: FAILED (${failed_step})")
        list(APPEND clargs_consumer_failed "${mode} (${failed_step})")
    endif()
    set(clargs_consumer_results "${clargs_consumer_results}" PARENT_SCOPE)
    set(clargs_consumer_failed "${clargs_consumer_failed}" PARENT_SCOPE)
endfunction()

# clargs_consumer_find_installed(<file-name> <out-var>)
#
# Sets <out-var> to the path of the one file <file-name> in the moved install
# tree. The library directory may be lib, lib64 or another one, so the file
# is searched for. Stops with a FATAL_ERROR unless there is exactly one.
function(clargs_consumer_find_installed file_name out_var)
    file(GLOB_RECURSE found LIST_DIRECTORIES false "${clargs_consumer_prefix}/${file_name}")
    list(LENGTH found count)
    if(NOT count EQUAL 1)
        list(JOIN found "\n  " found_text)
        message(FATAL_ERROR
            "consumer check: failed in the step \"install\". The install tree\n"
            "  ${clargs_consumer_prefix}\n"
            "must hold exactly one ${file_name}, and holds ${count}:\n"
            "  ${found_text}\n"
            "(A build directory with an absolute CMAKE_INSTALL_LIBDIR installs "
            "it outside the tree, and cannot be checked.)")
    endif()
    set(${out_var} "${found}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# The arguments.
# ---------------------------------------------------------------------------

foreach(clargs_consumer_argument IN ITEMS CLARGS_BUILD_DIR CLARGS_CONFIG CLARGS_SHARED)
    if(NOT DEFINED ${clargs_consumer_argument} OR "${${clargs_consumer_argument}}" STREQUAL "")
        message(FATAL_ERROR
            "consumer check: ${clargs_consumer_argument} is not set.\n"
            "${clargs_consumer_usage}")
    endif()
endforeach()

# A relative path is relative to the directory the script is run from.
get_filename_component(CLARGS_BUILD_DIR "${CLARGS_BUILD_DIR}" ABSOLUTE)
if(NOT EXISTS "${CLARGS_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR
        "consumer check: CLARGS_BUILD_DIR is not a configured build directory "
        "(it has no CMakeCache.txt):\n"
        "  ${CLARGS_BUILD_DIR}\n"
        "Configure and build CLArgs first.")
endif()

string(TOUPPER "${CLARGS_SHARED}" clargs_consumer_shared_upper)
if(clargs_consumer_shared_upper MATCHES "^(ON|TRUE|YES|1)$")
    set(clargs_consumer_shared ON)
    set(clargs_consumer_shape "shared")
elseif(clargs_consumer_shared_upper MATCHES "^(OFF|FALSE|NO|0)$")
    set(clargs_consumer_shared OFF)
    set(clargs_consumer_shape "static")
else()
    message(FATAL_ERROR
        "consumer check: CLARGS_SHARED is \"${CLARGS_SHARED}\". "
        "It must be ON (a shared library) or OFF (a static library).")
endif()

# What the build directory was configured with.
load_cache("${CLARGS_BUILD_DIR}" READ_WITH_PREFIX clargs_consumer_cache_
    CMAKE_PROJECT_NAME
    CLARGS_INSTALL
    BUILD_SHARED_LIBS
    CMAKE_BUILD_TYPE
    CMAKE_CONFIGURATION_TYPES
    CMAKE_GENERATOR
    CMAKE_GENERATOR_PLATFORM
    CMAKE_GENERATOR_TOOLSET
    CMAKE_GENERATOR_INSTANCE
    CMAKE_MAKE_PROGRAM
    CMAKE_C_COMPILER
)

if(NOT clargs_consumer_cache_CMAKE_PROJECT_NAME STREQUAL "CLArgs")
    message(FATAL_ERROR
        "consumer check: CLARGS_BUILD_DIR is not a build directory of CLArgs "
        "(its top-level project is \"${clargs_consumer_cache_CMAKE_PROJECT_NAME}\"):\n"
        "  ${CLARGS_BUILD_DIR}")
endif()

if(NOT clargs_consumer_cache_CLARGS_INSTALL)
    message(FATAL_ERROR
        "consumer check: the build directory\n"
        "  ${CLARGS_BUILD_DIR}\n"
        "was configured with CLARGS_INSTALL=${clargs_consumer_cache_CLARGS_INSTALL}, "
        "so it has no install rules and nothing can be installed. "
        "Configure it with -DCLARGS_INSTALL=ON.")
endif()

if(clargs_consumer_cache_BUILD_SHARED_LIBS)
    set(clargs_consumer_built_shape "shared")
else()
    set(clargs_consumer_built_shape "static")
endif()
if(NOT clargs_consumer_shape STREQUAL clargs_consumer_built_shape)
    message(FATAL_ERROR
        "consumer check: CLARGS_SHARED is ${CLARGS_SHARED} (a ${clargs_consumer_shape} "
        "library), and the build directory\n"
        "  ${CLARGS_BUILD_DIR}\n"
        "builds a ${clargs_consumer_built_shape} library "
        "(BUILD_SHARED_LIBS=${clargs_consumer_cache_BUILD_SHARED_LIBS}).")
endif()

if(clargs_consumer_cache_CMAKE_CONFIGURATION_TYPES)
    set(clargs_consumer_multi_config TRUE)
    set(clargs_consumer_configs ${clargs_consumer_cache_CMAKE_CONFIGURATION_TYPES})
    set(clargs_consumer_configs_from "CMAKE_CONFIGURATION_TYPES")
else()
    set(clargs_consumer_multi_config FALSE)
    set(clargs_consumer_configs "${clargs_consumer_cache_CMAKE_BUILD_TYPE}")
    set(clargs_consumer_configs_from "CMAKE_BUILD_TYPE")
endif()
if(NOT CLARGS_CONFIG IN_LIST clargs_consumer_configs)
    list(JOIN clargs_consumer_configs ", " clargs_consumer_configs_text)
    message(FATAL_ERROR
        "consumer check: CLARGS_CONFIG is \"${CLARGS_CONFIG}\", which is not a "
        "configuration of the build directory\n"
        "  ${CLARGS_BUILD_DIR}\n"
        "Its ${clargs_consumer_configs_from} is \"${clargs_consumer_configs_text}\".")
endif()

# ---------------------------------------------------------------------------
# The places, and how the consumer is configured.
# ---------------------------------------------------------------------------

get_filename_component(clargs_consumer_source_dir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(clargs_consumer_work_dir "${CLARGS_BUILD_DIR}/consumer-check")
set(clargs_consumer_staging "${clargs_consumer_work_dir}/install-staging")
set(clargs_consumer_prefix "${clargs_consumer_work_dir}/install-moved")
set(clargs_consumer_project_dir "${clargs_consumer_work_dir}/consumer")

# The generator and the compiler of the build directory. A Visual Studio
# generator chooses the compiler itself, from the platform and the toolset.
set(clargs_consumer_toolchain_arguments -G "${clargs_consumer_cache_CMAKE_GENERATOR}")
if(clargs_consumer_cache_CMAKE_GENERATOR_PLATFORM)
    list(APPEND clargs_consumer_toolchain_arguments
        -A "${clargs_consumer_cache_CMAKE_GENERATOR_PLATFORM}")
endif()
if(clargs_consumer_cache_CMAKE_GENERATOR_TOOLSET)
    list(APPEND clargs_consumer_toolchain_arguments
        -T "${clargs_consumer_cache_CMAKE_GENERATOR_TOOLSET}")
endif()
if(clargs_consumer_cache_CMAKE_GENERATOR_INSTANCE)
    list(APPEND clargs_consumer_toolchain_arguments
        "-DCMAKE_GENERATOR_INSTANCE=${clargs_consumer_cache_CMAKE_GENERATOR_INSTANCE}")
endif()
if(NOT clargs_consumer_cache_CMAKE_GENERATOR MATCHES "^Visual Studio")
    if(clargs_consumer_cache_CMAKE_MAKE_PROGRAM)
        list(APPEND clargs_consumer_toolchain_arguments
            "-DCMAKE_MAKE_PROGRAM=${clargs_consumer_cache_CMAKE_MAKE_PROGRAM}")
    endif()
    if(clargs_consumer_cache_CMAKE_C_COMPILER)
        list(APPEND clargs_consumer_toolchain_arguments
            "-DCMAKE_C_COMPILER=${clargs_consumer_cache_CMAKE_C_COMPILER}")
    endif()
endif()
if(NOT clargs_consumer_multi_config)
    list(APPEND clargs_consumer_toolchain_arguments "-DCMAKE_BUILD_TYPE=${CLARGS_CONFIG}")
endif()

message(STATUS
    "consumer check: ${CLARGS_CONFIG}, ${clargs_consumer_shape}, "
    "${clargs_consumer_cache_CMAKE_GENERATOR}")
message(STATUS "consumer check: build directory ${CLARGS_BUILD_DIR}")
message(STATUS "consumer check: work directory  ${clargs_consumer_work_dir}")

# Nothing of an earlier run is left.
file(REMOVE_RECURSE "${clargs_consumer_work_dir}")
file(MAKE_DIRECTORY "${clargs_consumer_work_dir}")

# ---------------------------------------------------------------------------
# Install, and move the install tree.
# ---------------------------------------------------------------------------

clargs_consumer_run(clargs_consumer_result clargs_consumer_text
    "${CMAKE_COMMAND}" --install "${CLARGS_BUILD_DIR}"
    --config "${CLARGS_CONFIG}" --prefix "${clargs_consumer_staging}"
)
if(NOT clargs_consumer_result STREQUAL "0")
    message(STATUS "consumer check: install FAILED.\n${clargs_consumer_text}")
    message(FATAL_ERROR
        "consumer check: failed in the step \"install\"; the output of the "
        "command is above. Is the configuration ${CLARGS_CONFIG} of the build "
        "directory built? If not, run:\n"
        "  cmake --build \"${CLARGS_BUILD_DIR}\" --config ${CLARGS_CONFIG}")
endif()

file(RENAME "${clargs_consumer_staging}" "${clargs_consumer_prefix}")
message(STATUS "consumer check: installed, and moved to ${clargs_consumer_prefix}")

clargs_consumer_find_installed(CLArgsConfig.cmake clargs_consumer_config_file)
clargs_consumer_find_installed(clargs.pc clargs_consumer_pc_file)
get_filename_component(clargs_consumer_config_dir "${clargs_consumer_config_file}" DIRECTORY)
get_filename_component(clargs_consumer_pc_dir "${clargs_consumer_pc_file}" DIRECTORY)

# ---------------------------------------------------------------------------
# The consumer project, outside the repository, and the four modes.
# ---------------------------------------------------------------------------

file(COPY "${CMAKE_CURRENT_LIST_DIR}/"
    DESTINATION "${clargs_consumer_project_dir}"
    PATTERN "run.cmake" EXCLUDE
)

set(clargs_consumer_results "")
set(clargs_consumer_failed "")

# find_package: a user gives the install prefix. CMake does not search every
# library directory below a prefix (lib64 on a Debian system, for one), so
# any other layout than <prefix>/lib gives the directory of the package.
if(clargs_consumer_config_dir STREQUAL "${clargs_consumer_prefix}/lib/cmake/CLArgs")
    clargs_consumer_check_mode(find_package "-DCMAKE_PREFIX_PATH=${clargs_consumer_prefix}")
else()
    clargs_consumer_check_mode(find_package "-DCLArgs_DIR=${clargs_consumer_config_dir}")
endif()

# pkg_config: only the moved clargs.pc is offered to pkg-config.
if(CMAKE_HOST_WIN32)
    message(STATUS
        "consumer check: pkg_config skipped on Windows; "
        "clargs.pc is installed: ${clargs_consumer_pc_file}")
    list(APPEND clargs_consumer_results "  pkg_config: skipped (Windows, clargs.pc is installed)")
else()
    set(clargs_consumer_saved_pkg_config_path "$ENV{PKG_CONFIG_PATH}")
    set(ENV{PKG_CONFIG_PATH} "${clargs_consumer_pc_dir}")
    clargs_consumer_check_mode(pkg_config)
    set(ENV{PKG_CONFIG_PATH} "${clargs_consumer_saved_pkg_config_path}")
endif()

# The embedded modes: this checkout, in the shape of the build directory.
foreach(clargs_consumer_mode IN ITEMS add_subdirectory fetch_content)
    clargs_consumer_check_mode(${clargs_consumer_mode}
        "-DCLARGS_CONSUME_SOURCE_DIR=${clargs_consumer_source_dir}"
        "-DBUILD_SHARED_LIBS=${clargs_consumer_shared}"
    )
endforeach()

# ---------------------------------------------------------------------------
# The result.
# ---------------------------------------------------------------------------

list(JOIN clargs_consumer_results "\n" clargs_consumer_results_text)
set(clargs_consumer_summary
    "consumer check: ${CLARGS_CONFIG}, ${clargs_consumer_shape}\n${clargs_consumer_results_text}")

if(clargs_consumer_failed)
    list(JOIN clargs_consumer_failed ", " clargs_consumer_failed_text)
    message(FATAL_ERROR
        "${clargs_consumer_summary}\n"
        "consumer check: failed in the mode(s): ${clargs_consumer_failed_text}. "
        "The output of each failed command is above.")
endif()

message(STATUS "${clargs_consumer_summary}")
message(STATUS "consumer check: all modes passed")
