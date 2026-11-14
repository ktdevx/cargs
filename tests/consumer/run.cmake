# run.cmake - runs the consumer check from start to end.
#
# A script, run with "cmake -P" on a build directory of CLine that is
# already built. CI calls it, and a developer runs the same command:
#
#   cmake -D CLINE_BUILD_DIR=<dir> -D CLINE_CONFIG=<Debug|Release>
#         -D CLINE_SHARED=<ON|OFF> -P tests/consumer/run.cmake
#
#   CLINE_BUILD_DIR  The build directory of CLine, configured with
#                     CLINE_INSTALL=ON and built. Required.
#   CLINE_CONFIG     The configuration to install and to build the consumer
#                     with. Required. It must be a configuration of the build
#                     directory: its CMAKE_BUILD_TYPE, or one of its
#                     CMAKE_CONFIGURATION_TYPES.
#   CLINE_SHARED     ON if the build directory builds a shared library, OFF
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
# Everything is written below <CLINE_BUILD_DIR>/consumer-check, which is
# removed and made again on every run. Nothing is written to the source tree.
#
# The consumer is built with the generator and the compiler of the build
# directory. The first two modes use the moved install tree, the last two
# this checkout (no network).
#
# The mode pkg_config runs on Linux and macOS. On Windows the script only
# checks that cline.pc is installed, and reports the mode as skipped.
#
# Every mode runs, also after another one failed. The script ends with 0 if
# all of them passed. Otherwise it prints the command that failed with its
# output, and ends with a FATAL_ERROR that names the modes that failed.
# A wrong argument and a failed install end the script at once.

cmake_minimum_required(VERSION 3.21)

set(cline_consumer_usage
    "Usage:\n"
    "  cmake -D CLINE_BUILD_DIR=<dir> -D CLINE_CONFIG=<Debug|Release> "
    "-D CLINE_SHARED=<ON|OFF> -P ${CMAKE_CURRENT_LIST_FILE}")
list(JOIN cline_consumer_usage "" cline_consumer_usage)

# cline_consumer_run(<result-var> <text-var> <command> [<argument>...])
#
# Runs the command. Sets <result-var> to its exit code (or to the reason why
# it did not run), and <text-var> to the command line followed by all that
# the command printed.
function(cline_consumer_run result_var text_var)
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

# cline_consumer_check_mode(<mode> [<configure-argument>...])
#
# Configures, builds and runs the copy of the consumer project for <mode>.
# Appends a line for the mode to cline_consumer_results and, if a step
# failed, the mode and the step to cline_consumer_failed.
function(cline_consumer_check_mode mode)
    set(build_dir "${cline_consumer_work_dir}/build-${mode}")
    message(STATUS "consumer check: ${mode} ...")

    set(failed_step "")
    foreach(step IN ITEMS configure build run)
        if(step STREQUAL "configure")
            cline_consumer_run(result text
                "${CMAKE_COMMAND}"
                -S "${cline_consumer_project_dir}"
                -B "${build_dir}"
                ${cline_consumer_toolchain_arguments}
                "-DCLINE_CONSUME_MODE=${mode}"
                ${ARGN}
            )
        elseif(step STREQUAL "build")
            cline_consumer_run(result text
                "${CMAKE_COMMAND}" --build "${build_dir}" --config "${CLINE_CONFIG}"
            )
        else()
            # The consumer registers its executable with CTest, so its path
            # does not matter. No test at all is a failure, not a success.
            cline_consumer_run(result text
                "${CMAKE_CTEST_COMMAND}" --test-dir "${build_dir}" -C "${CLINE_CONFIG}"
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
        list(APPEND cline_consumer_results "  ${mode}: passed")
    else()
        message(STATUS "consumer check: ${mode} FAILED in the step \"${failed_step}\".\n${text}")
        list(APPEND cline_consumer_results "  ${mode}: FAILED (${failed_step})")
        list(APPEND cline_consumer_failed "${mode} (${failed_step})")
    endif()
    set(cline_consumer_results "${cline_consumer_results}" PARENT_SCOPE)
    set(cline_consumer_failed "${cline_consumer_failed}" PARENT_SCOPE)
endfunction()

# cline_consumer_find_installed(<file-name> <out-var>)
#
# Sets <out-var> to the path of the one file <file-name> in the moved install
# tree. The library directory may be lib, lib64 or another one, so the file
# is searched for. Stops with a FATAL_ERROR unless there is exactly one.
function(cline_consumer_find_installed file_name out_var)
    file(GLOB_RECURSE found LIST_DIRECTORIES false "${cline_consumer_prefix}/${file_name}")
    list(LENGTH found count)
    if(NOT count EQUAL 1)
        list(JOIN found "\n  " found_text)
        message(FATAL_ERROR
            "consumer check: failed in the step \"install\". The install tree\n"
            "  ${cline_consumer_prefix}\n"
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

foreach(cline_consumer_argument IN ITEMS CLINE_BUILD_DIR CLINE_CONFIG CLINE_SHARED)
    if(NOT DEFINED ${cline_consumer_argument} OR "${${cline_consumer_argument}}" STREQUAL "")
        message(FATAL_ERROR
            "consumer check: ${cline_consumer_argument} is not set.\n"
            "${cline_consumer_usage}")
    endif()
endforeach()

# A relative path is relative to the directory the script is run from.
get_filename_component(CLINE_BUILD_DIR "${CLINE_BUILD_DIR}" ABSOLUTE)
if(NOT EXISTS "${CLINE_BUILD_DIR}/CMakeCache.txt")
    message(FATAL_ERROR
        "consumer check: CLINE_BUILD_DIR is not a configured build directory "
        "(it has no CMakeCache.txt):\n"
        "  ${CLINE_BUILD_DIR}\n"
        "Configure and build CLine first.")
endif()

string(TOUPPER "${CLINE_SHARED}" cline_consumer_shared_upper)
if(cline_consumer_shared_upper MATCHES "^(ON|TRUE|YES|1)$")
    set(cline_consumer_shared ON)
    set(cline_consumer_shape "shared")
elseif(cline_consumer_shared_upper MATCHES "^(OFF|FALSE|NO|0)$")
    set(cline_consumer_shared OFF)
    set(cline_consumer_shape "static")
else()
    message(FATAL_ERROR
        "consumer check: CLINE_SHARED is \"${CLINE_SHARED}\". "
        "It must be ON (a shared library) or OFF (a static library).")
endif()

# What the build directory was configured with.
load_cache("${CLINE_BUILD_DIR}" READ_WITH_PREFIX cline_consumer_cache_
    CMAKE_PROJECT_NAME
    CLINE_INSTALL
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

if(NOT cline_consumer_cache_CMAKE_PROJECT_NAME STREQUAL "CLine")
    message(FATAL_ERROR
        "consumer check: CLINE_BUILD_DIR is not a build directory of CLine "
        "(its top-level project is \"${cline_consumer_cache_CMAKE_PROJECT_NAME}\"):\n"
        "  ${CLINE_BUILD_DIR}")
endif()

if(NOT cline_consumer_cache_CLINE_INSTALL)
    message(FATAL_ERROR
        "consumer check: the build directory\n"
        "  ${CLINE_BUILD_DIR}\n"
        "was configured with CLINE_INSTALL=${cline_consumer_cache_CLINE_INSTALL}, "
        "so it has no install rules and nothing can be installed. "
        "Configure it with -DCLINE_INSTALL=ON.")
endif()

if(cline_consumer_cache_BUILD_SHARED_LIBS)
    set(cline_consumer_built_shape "shared")
else()
    set(cline_consumer_built_shape "static")
endif()
if(NOT cline_consumer_shape STREQUAL cline_consumer_built_shape)
    message(FATAL_ERROR
        "consumer check: CLINE_SHARED is ${CLINE_SHARED} (a ${cline_consumer_shape} "
        "library), and the build directory\n"
        "  ${CLINE_BUILD_DIR}\n"
        "builds a ${cline_consumer_built_shape} library "
        "(BUILD_SHARED_LIBS=${cline_consumer_cache_BUILD_SHARED_LIBS}).")
endif()

if(cline_consumer_cache_CMAKE_CONFIGURATION_TYPES)
    set(cline_consumer_multi_config TRUE)
    set(cline_consumer_configs ${cline_consumer_cache_CMAKE_CONFIGURATION_TYPES})
    set(cline_consumer_configs_from "CMAKE_CONFIGURATION_TYPES")
else()
    set(cline_consumer_multi_config FALSE)
    set(cline_consumer_configs "${cline_consumer_cache_CMAKE_BUILD_TYPE}")
    set(cline_consumer_configs_from "CMAKE_BUILD_TYPE")
endif()
if(NOT CLINE_CONFIG IN_LIST cline_consumer_configs)
    list(JOIN cline_consumer_configs ", " cline_consumer_configs_text)
    message(FATAL_ERROR
        "consumer check: CLINE_CONFIG is \"${CLINE_CONFIG}\", which is not a "
        "configuration of the build directory\n"
        "  ${CLINE_BUILD_DIR}\n"
        "Its ${cline_consumer_configs_from} is \"${cline_consumer_configs_text}\".")
endif()

# ---------------------------------------------------------------------------
# The places, and how the consumer is configured.
# ---------------------------------------------------------------------------

get_filename_component(cline_consumer_source_dir "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(cline_consumer_work_dir "${CLINE_BUILD_DIR}/consumer-check")
set(cline_consumer_staging "${cline_consumer_work_dir}/install-staging")
set(cline_consumer_prefix "${cline_consumer_work_dir}/install-moved")
set(cline_consumer_project_dir "${cline_consumer_work_dir}/consumer")

# The generator and the compiler of the build directory. A Visual Studio
# generator chooses the compiler itself, from the platform and the toolset.
set(cline_consumer_toolchain_arguments -G "${cline_consumer_cache_CMAKE_GENERATOR}")
if(cline_consumer_cache_CMAKE_GENERATOR_PLATFORM)
    list(APPEND cline_consumer_toolchain_arguments
        -A "${cline_consumer_cache_CMAKE_GENERATOR_PLATFORM}")
endif()
if(cline_consumer_cache_CMAKE_GENERATOR_TOOLSET)
    list(APPEND cline_consumer_toolchain_arguments
        -T "${cline_consumer_cache_CMAKE_GENERATOR_TOOLSET}")
endif()
if(cline_consumer_cache_CMAKE_GENERATOR_INSTANCE)
    list(APPEND cline_consumer_toolchain_arguments
        "-DCMAKE_GENERATOR_INSTANCE=${cline_consumer_cache_CMAKE_GENERATOR_INSTANCE}")
endif()
if(NOT cline_consumer_cache_CMAKE_GENERATOR MATCHES "^Visual Studio")
    if(cline_consumer_cache_CMAKE_MAKE_PROGRAM)
        list(APPEND cline_consumer_toolchain_arguments
            "-DCMAKE_MAKE_PROGRAM=${cline_consumer_cache_CMAKE_MAKE_PROGRAM}")
    endif()
    if(cline_consumer_cache_CMAKE_C_COMPILER)
        list(APPEND cline_consumer_toolchain_arguments
            "-DCMAKE_C_COMPILER=${cline_consumer_cache_CMAKE_C_COMPILER}")
    endif()
endif()
if(NOT cline_consumer_multi_config)
    list(APPEND cline_consumer_toolchain_arguments "-DCMAKE_BUILD_TYPE=${CLINE_CONFIG}")
endif()

message(STATUS
    "consumer check: ${CLINE_CONFIG}, ${cline_consumer_shape}, "
    "${cline_consumer_cache_CMAKE_GENERATOR}")
message(STATUS "consumer check: build directory ${CLINE_BUILD_DIR}")
message(STATUS "consumer check: work directory  ${cline_consumer_work_dir}")

# Nothing of an earlier run is left.
file(REMOVE_RECURSE "${cline_consumer_work_dir}")
file(MAKE_DIRECTORY "${cline_consumer_work_dir}")

# ---------------------------------------------------------------------------
# Install, and move the install tree.
# ---------------------------------------------------------------------------

cline_consumer_run(cline_consumer_result cline_consumer_text
    "${CMAKE_COMMAND}" --install "${CLINE_BUILD_DIR}"
    --config "${CLINE_CONFIG}" --prefix "${cline_consumer_staging}"
)
if(NOT cline_consumer_result STREQUAL "0")
    message(STATUS "consumer check: install FAILED.\n${cline_consumer_text}")
    message(FATAL_ERROR
        "consumer check: failed in the step \"install\"; the output of the "
        "command is above. Is the configuration ${CLINE_CONFIG} of the build "
        "directory built? If not, run:\n"
        "  cmake --build \"${CLINE_BUILD_DIR}\" --config ${CLINE_CONFIG}")
endif()

file(RENAME "${cline_consumer_staging}" "${cline_consumer_prefix}")
message(STATUS "consumer check: installed, and moved to ${cline_consumer_prefix}")

cline_consumer_find_installed(CLineConfig.cmake cline_consumer_config_file)
cline_consumer_find_installed(cline.pc cline_consumer_pc_file)
get_filename_component(cline_consumer_config_dir "${cline_consumer_config_file}" DIRECTORY)
get_filename_component(cline_consumer_pc_dir "${cline_consumer_pc_file}" DIRECTORY)

# ---------------------------------------------------------------------------
# The consumer project, outside the repository, and the four modes.
# ---------------------------------------------------------------------------

file(COPY "${CMAKE_CURRENT_LIST_DIR}/"
    DESTINATION "${cline_consumer_project_dir}"
    PATTERN "run.cmake" EXCLUDE
)

set(cline_consumer_results "")
set(cline_consumer_failed "")

# find_package: a user gives the install prefix. CMake does not search every
# library directory below a prefix (lib64 on a Debian system, for one), so
# any other layout than <prefix>/lib gives the directory of the package.
if(cline_consumer_config_dir STREQUAL "${cline_consumer_prefix}/lib/cmake/CLine")
    cline_consumer_check_mode(find_package "-DCMAKE_PREFIX_PATH=${cline_consumer_prefix}")
else()
    cline_consumer_check_mode(find_package "-DCLine_DIR=${cline_consumer_config_dir}")
endif()

# pkg_config: only the moved cline.pc is offered to pkg-config.
if(CMAKE_HOST_WIN32)
    message(STATUS
        "consumer check: pkg_config skipped on Windows; "
        "cline.pc is installed: ${cline_consumer_pc_file}")
    list(APPEND cline_consumer_results "  pkg_config: skipped (Windows, cline.pc is installed)")
else()
    set(cline_consumer_saved_pkg_config_path "$ENV{PKG_CONFIG_PATH}")
    set(ENV{PKG_CONFIG_PATH} "${cline_consumer_pc_dir}")
    cline_consumer_check_mode(pkg_config)
    set(ENV{PKG_CONFIG_PATH} "${cline_consumer_saved_pkg_config_path}")
endif()

# The embedded modes: this checkout, in the shape of the build directory.
foreach(cline_consumer_mode IN ITEMS add_subdirectory fetch_content)
    cline_consumer_check_mode(${cline_consumer_mode}
        "-DCLINE_CONSUME_SOURCE_DIR=${cline_consumer_source_dir}"
        "-DBUILD_SHARED_LIBS=${cline_consumer_shared}"
    )
endforeach()

# ---------------------------------------------------------------------------
# The result.
# ---------------------------------------------------------------------------

list(JOIN cline_consumer_results "\n" cline_consumer_results_text)
set(cline_consumer_summary
    "consumer check: ${CLINE_CONFIG}, ${cline_consumer_shape}\n${cline_consumer_results_text}")

if(cline_consumer_failed)
    list(JOIN cline_consumer_failed ", " cline_consumer_failed_text)
    message(FATAL_ERROR
        "${cline_consumer_summary}\n"
        "consumer check: failed in the mode(s): ${cline_consumer_failed_text}. "
        "The output of each failed command is above.")
endif()

message(STATUS "${cline_consumer_summary}")
message(STATUS "consumer check: all modes passed")
