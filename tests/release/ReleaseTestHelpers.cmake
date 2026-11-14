# ReleaseTestHelpers.cmake - checks and fixtures for the tests of the release
# scripts and modules (tests/release/*_test.cmake).
#
# Each test is a script, run with "cmake -P" by CTest (see tests/CMakeLists.txt)
# or by hand:
#
#   cmake -D CLINE_SOURCE_ROOT=<repository> -D CLINE_TEST_WORK_DIR=<directory>
#         -P tests/release/<name>_test.cmake
#
#   CLINE_SOURCE_ROOT    The root of the source tree under test.
#   CLINE_TEST_WORK_DIR  A directory the test owns. Including this file empties
#                         it, so every run starts from nothing. The test writes
#                         only there.
#
# Including this file also includes tools/release/ClineReadBytes.cmake, so
# that a test writes the files whose bytes it checks with cline_write_bytes(),
# which keeps every byte on Windows too (file(WRITE) turns LF into CRLF there).
# When the environment variable CLINE_WRITE_BYTES_WITH_CONFIGURE is set to a
# true value, the variable of that name is set, and cline_test_run_script()
# passes it on to the script it runs: every file is then written as on
# Windows, on any host.
#
# A test includes this file, runs its checks and ends with
# cline_test_finish(). A failed check does not stop the test: every failure
# is collected and printed at the end, and then the test stops with a
# FATAL_ERROR. A test that ran no check fails too, so that a test that skips
# everything by mistake is not taken for a passing one.
#
#   cline_test_expect_equal(<actual> <expected> <description>)
#       Passes when the two strings are equal.
#   cline_test_expect_contains(<text> <part> <description>)
#       Passes when <part> occurs in <text>.
#   cline_test_expect_not_contains(<text> <part> <description>)
#       Passes when <part> does not occur in <text>.
#   cline_test_copy_fixture(<fixture> <out-var> [<name>])
#       Copies tests/release/fixtures/<fixture> to the work directory, as
#       <name> when given, and sets <out-var> to the path of the copy.
#   cline_test_run_script(<prefix> <script> [<argument>...])
#       Runs "cmake <argument>... -P <script>" in a child process, from the work
#       directory (with -DCLINE_WRITE_BYTES_WITH_CONFIGURE=ON first when that
#       variable is true). Sets <prefix>_RESULT to its exit code and <prefix>_OUTPUT to
#       its standard output followed by its standard error.
#   cline_test_finish()
#       Prints the failures and stops with a FATAL_ERROR if there is one or if
#       no check ran.

include_guard(GLOBAL)

foreach(cline_test_var IN ITEMS CLINE_SOURCE_ROOT CLINE_TEST_WORK_DIR)
    if(NOT DEFINED ${cline_test_var} OR "${${cline_test_var}}" STREQUAL "")
        message(FATAL_ERROR
            "${cline_test_var} is not set. Run the test as\n"
            "  cmake -D CLINE_SOURCE_ROOT=<repository> -D CLINE_TEST_WORK_DIR=<directory> -P <test>")
    endif()
endforeach()

get_filename_component(CLINE_SOURCE_ROOT "${CLINE_SOURCE_ROOT}" ABSOLUTE)
get_filename_component(CLINE_TEST_WORK_DIR "${CLINE_TEST_WORK_DIR}" ABSOLUTE)
set(CLINE_TEST_FIXTURE_DIR "${CLINE_SOURCE_ROOT}/tests/release/fixtures")

include("${CLINE_SOURCE_ROOT}/tools/release/ClineReadBytes.cmake")
if("$ENV{CLINE_WRITE_BYTES_WITH_CONFIGURE}")
    set(CLINE_WRITE_BYTES_WITH_CONFIGURE ON)
endif()

# Emptying the work directory must never remove the source tree.
string(FIND "${CLINE_SOURCE_ROOT}/" "${CLINE_TEST_WORK_DIR}/" cline_test_pos)
if(cline_test_pos EQUAL 0)
    message(FATAL_ERROR
        "CLINE_TEST_WORK_DIR must not be the source tree or a directory above it:\n"
        "  ${CLINE_TEST_WORK_DIR}")
endif()
file(REMOVE_RECURSE "${CLINE_TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${CLINE_TEST_WORK_DIR}")

set_property(GLOBAL PROPERTY CLINE_TEST_CHECKS 0)
set_property(GLOBAL PROPERTY CLINE_TEST_FAILURES "")

# Counts one check, and records <message> as a failure unless <passed>.
function(_cline_test_record passed message)
    get_property(checks GLOBAL PROPERTY CLINE_TEST_CHECKS)
    math(EXPR checks "${checks} + 1")
    set_property(GLOBAL PROPERTY CLINE_TEST_CHECKS ${checks})
    if(NOT passed)
        # A string, not a list: a message may hold semicolons.
        set_property(GLOBAL APPEND_STRING PROPERTY CLINE_TEST_FAILURES "FAILED: ${message}\n")
    endif()
endfunction()

function(cline_test_expect_equal actual expected description)
    if("${actual}" STREQUAL "${expected}")
        _cline_test_record(TRUE "")
    else()
        _cline_test_record(FALSE
            "${description}\n  expected: [${expected}]\n  actual:   [${actual}]")
    endif()
endfunction()

function(cline_test_expect_contains text part description)
    string(FIND "${text}" "${part}" pos)
    if(pos EQUAL -1)
        _cline_test_record(FALSE
            "${description}\n  expected to contain: [${part}]\n  actual: [${text}]")
    else()
        _cline_test_record(TRUE "")
    endif()
endfunction()

function(cline_test_expect_not_contains text part description)
    string(FIND "${text}" "${part}" pos)
    if(pos EQUAL -1)
        _cline_test_record(TRUE "")
    else()
        _cline_test_record(FALSE
            "${description}\n  expected not to contain: [${part}]\n  actual: [${text}]")
    endif()
endfunction()

function(cline_test_copy_fixture fixture out_var)
    set(name "${fixture}")
    if(ARGC GREATER 2)
        set(name "${ARGV2}")
    endif()
    set(source "${CLINE_TEST_FIXTURE_DIR}/${fixture}")
    if(NOT EXISTS "${source}" OR IS_DIRECTORY "${source}")
        message(FATAL_ERROR "cline_test_copy_fixture: no fixture ${source}")
    endif()
    set(target "${CLINE_TEST_WORK_DIR}/${name}")
    get_filename_component(target_dir "${target}" DIRECTORY)
    file(MAKE_DIRECTORY "${target_dir}")
    # configure_file with COPYONLY keeps the bytes, line endings included.
    configure_file("${source}" "${target}" COPYONLY)
    set(${out_var} "${target}" PARENT_SCOPE)
endfunction()

function(cline_test_run_script prefix script)
    set(options "")
    if(CLINE_WRITE_BYTES_WITH_CONFIGURE)
        set(options "-DCLINE_WRITE_BYTES_WITH_CONFIGURE=ON")
    endif()
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${options} ${ARGN} -P "${script}"
        WORKING_DIRECTORY "${CLINE_TEST_WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(${prefix}_RESULT "${result}" PARENT_SCOPE)
    set(${prefix}_OUTPUT "${output}${error}" PARENT_SCOPE)
endfunction()

function(cline_test_finish)
    get_property(checks GLOBAL PROPERTY CLINE_TEST_CHECKS)
    get_property(failures GLOBAL PROPERTY CLINE_TEST_FAILURES)
    if(checks EQUAL 0)
        message(FATAL_ERROR "No check ran. A test must run at least one check.")
    endif()
    if(NOT failures STREQUAL "")
        message("${failures}")
        message(FATAL_ERROR "Some of the ${checks} checks failed; see above.")
    endif()
    message(STATUS "All ${checks} checks passed.")
endfunction()
