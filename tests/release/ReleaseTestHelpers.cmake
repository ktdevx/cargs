# ReleaseTestHelpers.cmake - checks and fixtures for the tests of the release
# scripts and modules (tests/release/*_test.cmake).
#
# Each test is a script, run with "cmake -P" by CTest (see tests/CMakeLists.txt)
# or by hand:
#
#   cmake -D CLARGS_SOURCE_ROOT=<repository> -D CLARGS_TEST_WORK_DIR=<directory>
#         -P tests/release/<name>_test.cmake
#
#   CLARGS_SOURCE_ROOT    The root of the source tree under test.
#   CLARGS_TEST_WORK_DIR  A directory the test owns. Including this file empties
#                         it, so every run starts from nothing. The test writes
#                         only there.
#
# A test includes this file, runs its checks and ends with
# clargs_test_finish(). A failed check does not stop the test: every failure
# is collected and printed at the end, and then the test stops with a
# FATAL_ERROR. A test that ran no check fails too, so that a test that skips
# everything by mistake is not taken for a passing one.
#
#   clargs_test_expect_equal(<actual> <expected> <description>)
#       Passes when the two strings are equal.
#   clargs_test_expect_contains(<text> <part> <description>)
#       Passes when <part> occurs in <text>.
#   clargs_test_expect_not_contains(<text> <part> <description>)
#       Passes when <part> does not occur in <text>.
#   clargs_test_copy_fixture(<fixture> <out-var> [<name>])
#       Copies tests/release/fixtures/<fixture> to the work directory, as
#       <name> when given, and sets <out-var> to the path of the copy.
#   clargs_test_run_script(<prefix> <script> [<argument>...])
#       Runs "cmake <argument>... -P <script>" in a child process, from the work
#       directory. Sets <prefix>_RESULT to its exit code and <prefix>_OUTPUT to
#       its standard output followed by its standard error.
#   clargs_test_finish()
#       Prints the failures and stops with a FATAL_ERROR if there is one or if
#       no check ran.

include_guard(GLOBAL)

foreach(clargs_test_var IN ITEMS CLARGS_SOURCE_ROOT CLARGS_TEST_WORK_DIR)
    if(NOT DEFINED ${clargs_test_var} OR "${${clargs_test_var}}" STREQUAL "")
        message(FATAL_ERROR
            "${clargs_test_var} is not set. Run the test as\n"
            "  cmake -D CLARGS_SOURCE_ROOT=<repository> -D CLARGS_TEST_WORK_DIR=<directory> -P <test>")
    endif()
endforeach()

get_filename_component(CLARGS_SOURCE_ROOT "${CLARGS_SOURCE_ROOT}" ABSOLUTE)
get_filename_component(CLARGS_TEST_WORK_DIR "${CLARGS_TEST_WORK_DIR}" ABSOLUTE)
set(CLARGS_TEST_FIXTURE_DIR "${CLARGS_SOURCE_ROOT}/tests/release/fixtures")

# Emptying the work directory must never remove the source tree.
string(FIND "${CLARGS_SOURCE_ROOT}/" "${CLARGS_TEST_WORK_DIR}/" clargs_test_pos)
if(clargs_test_pos EQUAL 0)
    message(FATAL_ERROR
        "CLARGS_TEST_WORK_DIR must not be the source tree or a directory above it:\n"
        "  ${CLARGS_TEST_WORK_DIR}")
endif()
file(REMOVE_RECURSE "${CLARGS_TEST_WORK_DIR}")
file(MAKE_DIRECTORY "${CLARGS_TEST_WORK_DIR}")

set_property(GLOBAL PROPERTY CLARGS_TEST_CHECKS 0)
set_property(GLOBAL PROPERTY CLARGS_TEST_FAILURES "")

# Counts one check, and records <message> as a failure unless <passed>.
function(_clargs_test_record passed message)
    get_property(checks GLOBAL PROPERTY CLARGS_TEST_CHECKS)
    math(EXPR checks "${checks} + 1")
    set_property(GLOBAL PROPERTY CLARGS_TEST_CHECKS ${checks})
    if(NOT passed)
        # A string, not a list: a message may hold semicolons.
        set_property(GLOBAL APPEND_STRING PROPERTY CLARGS_TEST_FAILURES "FAILED: ${message}\n")
    endif()
endfunction()

function(clargs_test_expect_equal actual expected description)
    if("${actual}" STREQUAL "${expected}")
        _clargs_test_record(TRUE "")
    else()
        _clargs_test_record(FALSE
            "${description}\n  expected: [${expected}]\n  actual:   [${actual}]")
    endif()
endfunction()

function(clargs_test_expect_contains text part description)
    string(FIND "${text}" "${part}" pos)
    if(pos EQUAL -1)
        _clargs_test_record(FALSE
            "${description}\n  expected to contain: [${part}]\n  actual: [${text}]")
    else()
        _clargs_test_record(TRUE "")
    endif()
endfunction()

function(clargs_test_expect_not_contains text part description)
    string(FIND "${text}" "${part}" pos)
    if(pos EQUAL -1)
        _clargs_test_record(TRUE "")
    else()
        _clargs_test_record(FALSE
            "${description}\n  expected not to contain: [${part}]\n  actual: [${text}]")
    endif()
endfunction()

function(clargs_test_copy_fixture fixture out_var)
    set(name "${fixture}")
    if(ARGC GREATER 2)
        set(name "${ARGV2}")
    endif()
    set(source "${CLARGS_TEST_FIXTURE_DIR}/${fixture}")
    if(NOT EXISTS "${source}" OR IS_DIRECTORY "${source}")
        message(FATAL_ERROR "clargs_test_copy_fixture: no fixture ${source}")
    endif()
    set(target "${CLARGS_TEST_WORK_DIR}/${name}")
    get_filename_component(target_dir "${target}" DIRECTORY)
    file(MAKE_DIRECTORY "${target_dir}")
    # configure_file with COPYONLY keeps the bytes, line endings included.
    configure_file("${source}" "${target}" COPYONLY)
    set(${out_var} "${target}" PARENT_SCOPE)
endfunction()

function(clargs_test_run_script prefix script)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${ARGN} -P "${script}"
        WORKING_DIRECTORY "${CLARGS_TEST_WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(${prefix}_RESULT "${result}" PARENT_SCOPE)
    set(${prefix}_OUTPUT "${output}${error}" PARENT_SCOPE)
endfunction()

function(clargs_test_finish)
    get_property(checks GLOBAL PROPERTY CLARGS_TEST_CHECKS)
    get_property(failures GLOBAL PROPERTY CLARGS_TEST_FAILURES)
    if(checks EQUAL 0)
        message(FATAL_ERROR "No check ran. A test must run at least one check.")
    endif()
    if(NOT failures STREQUAL "")
        message("${failures}")
        message(FATAL_ERROR "Some of the ${checks} checks failed; see above.")
    endif()
    message(STATUS "All ${checks} checks passed.")
endfunction()
