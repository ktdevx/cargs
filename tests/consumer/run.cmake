# run.cmake - runs the consumer check from start to end.
#
# A script, run with "cmake -P" on a build directory of ArgX that is
# already built. CI calls it, and a developer runs the same command:
#
#   cmake -D ARGX_BUILD_DIR=<dir> -D ARGX_CONFIG=<Debug|Release>
#         -D ARGX_SHARED=<ON|OFF> -P tests/consumer/run.cmake
#
#   ARGX_BUILD_DIR  The build directory of ArgX, configured with
#                     ARGX_INSTALL=ON and built. Required.
#   ARGX_CONFIG     The configuration to install and to build the consumer
#                     with. Required. It must be a configuration of the build
#                     directory: its CMAKE_BUILD_TYPE, or one of its
#                     CMAKE_CONFIGURATION_TYPES.
#   ARGX_SHARED     ON if the build directory builds a shared library, OFF
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
# Everything is written below <ARGX_BUILD_DIR>/consumer-check, which is
# removed and made again on every run. Nothing is written to the source tree.
#
# The consumer is built with the generator and the compiler of the build
# directory. The first two modes use the moved install tree, the last two
# this checkout (no network).
#
# The mode pkg_config runs on Linux and macOS. On Windows the script only
# checks that argx.pc is installed, and reports the mode as skipped.
#
# Every mode runs, also after another one failed. The script ends with 0 if
# all of them passed. Otherwise it prints the command that failed with its
# output, and ends with a FATAL_ERROR that names the modes that failed.
# A wrong argument and a failed install end the script at once.

cmake_minimum_required(VERSION 3.21)

set(argx_consumer_usage
    "Usage:\n"
    "  cmake -D ARGX_BUILD_DIR=<dir> -D ARGX_CONFIG=<Debug|Release> "
    "-D ARGX_SHARED=<ON|OFF> -P ${CMAKE_CURRENT_LIST_FILE}")
list(JOIN argx_consumer_usage "" argx_consumer_usage)

# argx_consumer_run(<result-var> <text-var> <command> [<argument>...])
#
# Runs the command. Sets <result-var> to its exit code (or to the reason why
# it did not run), and <text-var> to the command line followed by all that
# the command printed.
function(argx_consumer_run result_var text_var)
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

# argx_consumer_check_mode(<mode> [<configure-argument>...])
#
# Configures, builds and runs the copy of the consumer project for <mode>.
# Appends a line for the mode to argx_consumer_results and, if a step
# failed, the mode and the step to argx_consumer_failed.
function(argx_consumer_check_mode mode)
    set(build_dir "${argx_consumer_work_dir}/build-${mode}")
    message(STATUS "consumer check: ${mode} ...")

    set(failed_step "")
    foreach(step IN ITEMS configure build run)
        if(step STREQUAL "configure")
            argx_consumer_run(result text
                "${CMAKE_COMMAND}"
                -S "${argx_consumer_project_dir}"
                -B "${build_dir}"
                ${argx_consumer_toolchain_arguments}
                "-DARGX_CONSUME_MODE=${mode}"
                ${ARGN}
            )
        elseif(step STREQUAL "build")
            argx_consumer_run(result text
                "${CMAKE_COMMAND}" --build "${build_dir}" --config "${ARGX_CONFIG}"
            )
        else()
            # The consumer registers its executable with CTest, so its path
            # does not matter. No test at all is a failure, not a success.
            argx_consumer_run(result text
                "${CMAKE_CTEST_COMMAND}" --test-dir "${build_dir}" -C "${ARGX_CONFIG}"
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
        list(APPEND argx_consumer_results "  ${mode}: passed")
    else()
        message(STATUS "consumer check: ${mode} FAILED in the step \"${failed_step}\".\n${text}")
        list(APPEND argx_consumer_results "  ${mode}: FAILED (${failed_step})")
        list(APPEND argx_consumer_failed "${mode} (${failed_step})")
    endif()
    set(argx_consumer_results "${argx_consumer_results}" PARENT_SCOPE)
    set(argx_consumer_failed "${argx_consumer_failed}" PARENT_SCOPE)
endfunction()

# argx_consumer_find_installed(<file-name> <out-var>)
#
# Sets <out-var> to the path of the one file <file-name> in the moved install
# tree. The library directory may be lib, lib64 or another one, so the file
# is searched for. Stops with a FATAL_ERROR unless there is exactly one.
function(argx_consumer_find_installed file_name out_var)
    file(GLOB_RECURSE found LIST_DIRECTORIES false "${argx_consumer_prefix}/${file_name}")
    list(LENGTH found count)
    if(NOT count EQUAL 1)
        list(JOIN found "\n  " found_text)
        message(FATAL_ERROR
            "consumer check: failed in the step \"install\". The install tree\n"
            "  ${argx_consumer_prefix}\n"
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

foreach(argx_consumer_argument IN ITEMS ARGX_BUILD_DIR ARGX_CONFIG ARGX_SHARED)
    if(NOT DEFINED ${argx_consumer_argument} OR "${${argx_consumer_argument}}" STREQUAL "")
        message(FATAL_ERROR
            "consumer check: ${argx_consumer_argument} is not set.\n"
            "${argx_consumer_usage}")
    endif()
endforeach()

# A relative path is relative to the directory the script is run from.
get_filename_component(ARGX_BUILD_DIR "${ARGX_BUILD_DIR}" ABSOLUTE)
if(NOT EXISTS "${ARGX_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR
        "consumer check: ARGX_BUILD_DIR is not a configured build directory "
        "(it has no CMakeCache.txt):\n"
        "  ${ARGX_BUILD_DIR}\n"
        "Configure and build ArgX first.")
endif()

string(TOUPPER "${ARGX_SHARED}" argx_consumer_shared_upper)
if(argx_consumer_shared_upper MATCHES "^(ON|TRUE|YES|1)$")
    set(argx_consumer_shared ON)
    set(argx_consumer_shape "shared")
elseif(argx_consumer_shared_upper MATCHES "^(OFF|FALSE|NO|0)$")
    set(argx_consumer_shared OFF)
    set(argx_consumer_shape "static")
else()
    message(FATAL_ERROR
        "consumer check: ARGX_SHARED is \"${ARGX_SHARED}\". "
        "It must be ON (a shared library) or OFF (a static library).")
endif()

# What the build directory was configured with.
load_cache("${ARGX_BUILD_DIR}" READ_WITH_PREFIX argx_consumer_cache_
    CMAKE_PROJECT_NAME
    ARGX_INSTALL
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

if(NOT argx_consumer_cache_CMAKE_PROJECT_NAME STREQUAL "ArgX")
    message(FATAL_ERROR
        "consumer check: ARGX_BUILD_DIR is not a build directory of ArgX "
        "(its top-level project is \"${argx_consumer_cache_CMAKE_PROJECT_NAME}\"):\n"
        "  ${ARGX_BUILD_DIR}")
endif()

if(NOT argx_consumer_cache_ARGX_INSTALL)
    message(FATAL_ERROR
        "consumer check: the build directory\n"
        "  ${ARGX_BUILD_DIR}\n"
        "was configured with ARGX_INSTALL=${argx_consumer_cache_ARGX_INSTALL}, "
        "so it has no install rules and nothing can be installed. "
        "Configure it with -DARGX_INSTALL=ON.")
endif()

if(argx_consumer_cache_BUILD_SHARED_LIBS)
    set(argx_consumer_built_shape "shared")
else()
    set(argx_consumer_built_shape "static")
endif()
if(NOT argx_consumer_shape STREQUAL argx_consumer_built_shape)
    message(FATAL_ERROR
        "consumer check: ARGX_SHARED is ${ARGX_SHARED} (a ${argx_consumer_shape} "
        "library), and the build directory\n"
        "  ${ARGX_BUILD_DIR}\n"
        "builds a ${argx_consumer_built_shape} library "
        "(BUILD_SHARED_LIBS=${argx_consumer_cache_BUILD_SHARED_LIBS}).")
endif()

if(argx_consumer_cache_CMAKE_CONFIGURATION_TYPES)
    set(argx_consumer_multi_config TRUE)
    set(argx_consumer_configs ${argx_consumer_cache_CMAKE_CONFIGURATION_TYPES})
    set(argx_consumer_configs_from "CMAKE_CONFIGURATION_TYPES")
else()
    set(argx_consumer_multi_config FALSE)
    set(argx_consumer_configs "${argx_consumer_cache_CMAKE_BUILD_TYPE}")
    set(argx_consumer_configs_from "CMAKE_BUILD_TYPE")
endif()
if(NOT ARGX_CONFIG IN_LIST argx_consumer_configs)
    list(JOIN argx_consumer_configs ", " argx_consumer_configs_text)
    message(FATAL_ERROR
        "consumer check: ARGX_CONFIG is \"${ARGX_CONFIG}\", which is not a "
        "configuration of the build directory\n"
        "  ${ARGX_BUILD_DIR}\n"
        "Its ${argx_consumer_configs_from} is \"${argx_consumer_configs_text}\".")
endif()

# ---------------------------------------------------------------------------
# The places, and how the consumer is configured.
# ---------------------------------------------------------------------------

get_filename_component(argx_consumer_source_dir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(argx_consumer_work_dir "${ARGX_BUILD_DIR}/consumer-check")
set(argx_consumer_staging "${argx_consumer_work_dir}/install-staging")
set(argx_consumer_prefix "${argx_consumer_work_dir}/install-moved")
set(argx_consumer_project_dir "${argx_consumer_work_dir}/consumer")

# The generator and the compiler of the build directory. A Visual Studio
# generator chooses the compiler itself, from the platform and the toolset.
set(argx_consumer_toolchain_arguments -G "${argx_consumer_cache_CMAKE_GENERATOR}")
if(argx_consumer_cache_CMAKE_GENERATOR_PLATFORM)
    list(APPEND argx_consumer_toolchain_arguments
        -A "${argx_consumer_cache_CMAKE_GENERATOR_PLATFORM}")
endif()
if(argx_consumer_cache_CMAKE_GENERATOR_TOOLSET)
    list(APPEND argx_consumer_toolchain_arguments
        -T "${argx_consumer_cache_CMAKE_GENERATOR_TOOLSET}")
endif()
if(argx_consumer_cache_CMAKE_GENERATOR_INSTANCE)
    list(APPEND argx_consumer_toolchain_arguments
        "-DCMAKE_GENERATOR_INSTANCE=${argx_consumer_cache_CMAKE_GENERATOR_INSTANCE}")
endif()
if(NOT argx_consumer_cache_CMAKE_GENERATOR MATCHES "^Visual Studio")
    if(argx_consumer_cache_CMAKE_MAKE_PROGRAM)
        list(APPEND argx_consumer_toolchain_arguments
            "-DCMAKE_MAKE_PROGRAM=${argx_consumer_cache_CMAKE_MAKE_PROGRAM}")
    endif()
    if(argx_consumer_cache_CMAKE_C_COMPILER)
        list(APPEND argx_consumer_toolchain_arguments
            "-DCMAKE_C_COMPILER=${argx_consumer_cache_CMAKE_C_COMPILER}")
    endif()
endif()
if(NOT argx_consumer_multi_config)
    list(APPEND argx_consumer_toolchain_arguments "-DCMAKE_BUILD_TYPE=${ARGX_CONFIG}")
endif()

message(STATUS
    "consumer check: ${ARGX_CONFIG}, ${argx_consumer_shape}, "
    "${argx_consumer_cache_CMAKE_GENERATOR}")
message(STATUS "consumer check: build directory ${ARGX_BUILD_DIR}")
message(STATUS "consumer check: work directory  ${argx_consumer_work_dir}")

# Nothing of an earlier run is left.
file(REMOVE_RECURSE "${argx_consumer_work_dir}")
file(MAKE_DIRECTORY "${argx_consumer_work_dir}")

# ---------------------------------------------------------------------------
# Install, and move the install tree.
# ---------------------------------------------------------------------------

argx_consumer_run(argx_consumer_result argx_consumer_text
    "${CMAKE_COMMAND}" --install "${ARGX_BUILD_DIR}"
    --config "${ARGX_CONFIG}" --prefix "${argx_consumer_staging}"
)
if(NOT argx_consumer_result STREQUAL "0")
    message(STATUS "consumer check: install FAILED.\n${argx_consumer_text}")
    message(FATAL_ERROR
        "consumer check: failed in the step \"install\"; the output of the "
        "command is above. Is the configuration ${ARGX_CONFIG} of the build "
        "directory built? If not, run:\n"
        "  cmake --build \"${ARGX_BUILD_DIR}\" --config ${ARGX_CONFIG}")
endif()

file(RENAME "${argx_consumer_staging}" "${argx_consumer_prefix}")
message(STATUS "consumer check: installed, and moved to ${argx_consumer_prefix}")

argx_consumer_find_installed(ArgXConfig.cmake argx_consumer_config_file)
argx_consumer_find_installed(argx.pc argx_consumer_pc_file)
get_filename_component(argx_consumer_config_dir "${argx_consumer_config_file}" DIRECTORY)
get_filename_component(argx_consumer_pc_dir "${argx_consumer_pc_file}" DIRECTORY)

# ---------------------------------------------------------------------------
# The consumer project, outside the repository, and the four modes.
# ---------------------------------------------------------------------------

file(COPY "${CMAKE_CURRENT_LIST_DIR}/"
    DESTINATION "${argx_consumer_project_dir}"
    PATTERN "run.cmake" EXCLUDE
)

set(argx_consumer_results "")
set(argx_consumer_failed "")

# find_package: a user gives the install prefix. CMake does not search every
# library directory below a prefix (lib64 on a Debian system, for one), so
# any other layout than <prefix>/lib gives the directory of the package.
if(argx_consumer_config_dir STREQUAL "${argx_consumer_prefix}/lib/cmake/ArgX")
    argx_consumer_check_mode(find_package "-DCMAKE_PREFIX_PATH=${argx_consumer_prefix}")
else()
    argx_consumer_check_mode(find_package "-DArgX_DIR=${argx_consumer_config_dir}")
endif()

# pkg_config: only the moved argx.pc is offered to pkg-config.
if(CMAKE_HOST_WIN32)
    message(STATUS
        "consumer check: pkg_config skipped on Windows; "
        "argx.pc is installed: ${argx_consumer_pc_file}")
    list(APPEND argx_consumer_results "  pkg_config: skipped (Windows, argx.pc is installed)")
else()
    set(argx_consumer_saved_pkg_config_path "$ENV{PKG_CONFIG_PATH}")
    set(ENV{PKG_CONFIG_PATH} "${argx_consumer_pc_dir}")
    argx_consumer_check_mode(pkg_config)
    set(ENV{PKG_CONFIG_PATH} "${argx_consumer_saved_pkg_config_path}")
endif()

# The embedded modes: this checkout, in the shape of the build directory.
foreach(argx_consumer_mode IN ITEMS add_subdirectory fetch_content)
    argx_consumer_check_mode(${argx_consumer_mode}
        "-DARGX_CONSUME_SOURCE_DIR=${argx_consumer_source_dir}"
        "-DBUILD_SHARED_LIBS=${argx_consumer_shared}"
    )
endforeach()

# ---------------------------------------------------------------------------
# The result.
# ---------------------------------------------------------------------------

list(JOIN argx_consumer_results "\n" argx_consumer_results_text)
set(argx_consumer_summary
    "consumer check: ${ARGX_CONFIG}, ${argx_consumer_shape}\n${argx_consumer_results_text}")

if(argx_consumer_failed)
    list(JOIN argx_consumer_failed ", " argx_consumer_failed_text)
    message(FATAL_ERROR
        "${argx_consumer_summary}\n"
        "consumer check: failed in the mode(s): ${argx_consumer_failed_text}. "
        "The output of each failed command is above.")
endif()

message(STATUS "${argx_consumer_summary}")
message(STATUS "consumer check: all modes passed")
