# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# Tests of tools/release/ArgXSemver.cmake: parsing a version of the form
# <major>.<minor>.<patch>[-<pre-release>], rejecting every other form with a
# reason, and comparing two versions by SemVer 2.0.0 precedence.
#
# Run by CTest as release.semver; see ReleaseTestHelpers.cmake.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ReleaseTestHelpers.cmake")
include("${ARGX_SOURCE_ROOT}/tools/release/ArgXSemver.cmake")

# --- Valid versions ----------------------------------------------------------

# Parses <version> and checks that it is valid with the given parts.
function(expect_valid version major minor patch prerelease)
    # Every output is set even when the variables held a value before.
    foreach(field IN ITEMS VALID ERROR MAJOR MINOR PATCH PRERELEASE CORE)
        set(v_${field} "stale")
    endforeach()
    argx_semver_parse("${version}" v)
    set(what "\"${version}\"")
    argx_test_expect_equal("${v_VALID}" "TRUE" "${what}: valid")
    argx_test_expect_equal("${v_ERROR}" "" "${what}: no error")
    argx_test_expect_equal("${v_MAJOR}" "${major}" "${what}: the major version")
    argx_test_expect_equal("${v_MINOR}" "${minor}" "${what}: the minor version")
    argx_test_expect_equal("${v_PATCH}" "${patch}" "${what}: the patch version")
    argx_test_expect_equal("${v_PRERELEASE}" "${prerelease}" "${what}: the pre-release identifiers")
    argx_test_expect_equal("${v_CORE}" "${major}.${minor}.${patch}" "${what}: the core")
endfunction()

expect_valid("0.0.0" 0 0 0 "")
expect_valid("1.2.3" 1 2 3 "")
expect_valid("10.20.30" 10 20 30 "")
expect_valid("255.255.255" 255 255 255 "")
expect_valid("0.1.0-rc.1" 0 1 0 "rc.1")
expect_valid("1.0.0-alpha" 1 0 0 "alpha")
expect_valid("1.0.0-alpha.beta.1" 1 0 0 "alpha.beta.1")
expect_valid("1.0.0-0" 1 0 0 "0")
expect_valid("1.0.0-0.3.7" 1 0 0 "0.3.7")
expect_valid("1.0.0-x.7.z.92" 1 0 0 "x.7.z.92")
expect_valid("1.0.0-x-y-z.--" 1 0 0 "x-y-z.--")
expect_valid("1.0.0-01a" 1 0 0 "01a")
expect_valid("1.0.0-rc.12345678901234567890" 1 0 0 "rc.12345678901234567890")

# --- Invalid versions --------------------------------------------------------

# Parses <version> and checks that it is invalid, that every part is empty,
# and that the reason names the version and holds <part>.
function(expect_invalid version part)
    foreach(field IN ITEMS VALID ERROR MAJOR MINOR PATCH PRERELEASE CORE)
        set(v_${field} "stale")
    endforeach()
    argx_semver_parse("${version}" v)
    set(what "\"${version}\"")
    argx_test_expect_equal("${v_VALID}" "FALSE" "${what}: invalid")
    argx_test_expect_contains("${v_ERROR}" "\"${version}\"" "${what}: the reason names the version")
    argx_test_expect_contains("${v_ERROR}" "${part}" "${what}: the reason")
    foreach(field IN ITEMS MAJOR MINOR PATCH PRERELEASE CORE)
        argx_test_expect_equal("${v_${field}}" "" "${what}: ${field} is empty")
    endforeach()
endfunction()

set(form "is not of the form <major>.<minor>.<patch>[-<pre-release>]")
expect_invalid("" "${form}")
expect_invalid("1" "${form}")
expect_invalid("1.2" "${form}")
expect_invalid("1.2.3.4" "${form}")
expect_invalid("v1.2.3" "${form}")
expect_invalid("a.b.c" "${form}")
expect_invalid("-1.2.3" "${form}")
expect_invalid("1..3" "${form}")
expect_invalid(" 1.2.3" "${form}")
expect_invalid("1.2.3 " "${form}")
expect_invalid("1.2.3\n" "${form}")
expect_invalid("1.2.3-rc.1\n" "not valid pre-release identifiers")

set(metadata "build metadata (\"+...\") is not accepted")
expect_invalid("1.2.3+b" "${metadata}")
expect_invalid("1.2.3-rc.1+build.5" "${metadata}")
expect_invalid("1.2.3+" "${metadata}")

set(leading "has a leading zero")
expect_invalid("01.2.3" "the major version \"01\" ${leading}")
expect_invalid("1.02.3" "the minor version \"02\" ${leading}")
expect_invalid("1.2.03" "the patch version \"03\" ${leading}")
expect_invalid("00.0.0" "the major version \"00\" ${leading}")

set(range "is not in the range 0-255")
expect_invalid("256.0.0" "the major version \"256\" ${range}")
expect_invalid("0.256.0" "the minor version \"256\" ${range}")
expect_invalid("0.0.256" "the patch version \"256\" ${range}")
expect_invalid("1000.0.0" "the major version \"1000\" ${range}")
# A number far beyond any integer CMake can compute with is rejected, not
# wrapped around.
expect_invalid("99999999999999999999.0.0" "the major version \"99999999999999999999\" ${range}")
expect_invalid("4294967296.0.0" "the major version \"4294967296\" ${range}")

expect_invalid("1.2.3-" "the pre-release identifiers are empty")
expect_invalid("1.2.3-rc..1" "has an empty pre-release identifier")
expect_invalid("1.2.3-.rc" "has an empty pre-release identifier")
expect_invalid("1.2.3-rc." "has an empty pre-release identifier")
expect_invalid("1.2.3-rc_1" "not valid pre-release identifiers")
expect_invalid("1.2.3-rc 1" "not valid pre-release identifiers")
expect_invalid("1.2.3-01" "the numeric pre-release identifier \"01\" has a leading zero")
expect_invalid("1.2.3-rc.007" "the numeric pre-release identifier \"007\" has a leading zero")

# Every problem is in the reason, not only the first.
argx_semver_parse("01.256.3" v)
argx_test_expect_contains("${v_ERROR}" "the major version \"01\" ${leading}" "two problems: the first")
argx_test_expect_contains("${v_ERROR}" "the minor version \"256\" ${range}" "two problems: the second")

# A semicolon does not split the version into a list.
argx_semver_parse("1.2.3-a;b" v)
argx_test_expect_equal("${v_VALID}" "FALSE" "\"1.2.3-a;b\": invalid")
argx_semver_parse("1;2;3" v)
argx_test_expect_equal("${v_VALID}" "FALSE" "\"1;2;3\": invalid")

# --- Precedence --------------------------------------------------------------

# Checks that <a> compares to <b> as <expected>, and <b> to <a> as the opposite.
function(expect_compare a b expected)
    argx_semver_compare("${a}" "${b}" result)
    argx_test_expect_equal("${result}" "${expected}" "compare(${a}, ${b})")
    math(EXPR opposite "0 - (${expected})")
    argx_semver_compare("${b}" "${a}" result)
    argx_test_expect_equal("${result}" "${opposite}" "compare(${b}, ${a})")
endfunction()

# The example of the SemVer 2.0.0 specification (item 11), in ascending order.
# Every pair is compared both ways, and every version with itself.
set(spec_order
    1.0.0-alpha
    1.0.0-alpha.1
    1.0.0-alpha.beta
    1.0.0-beta
    1.0.0-beta.2
    1.0.0-beta.11
    1.0.0-rc.1
    1.0.0
)
list(LENGTH spec_order spec_count)
math(EXPR spec_last "${spec_count} - 1")
foreach(i RANGE ${spec_last})
    list(GET spec_order ${i} a)
    expect_compare("${a}" "${a}" 0)
    math(EXPR next "${i} + 1")
    if(next LESS_EQUAL spec_last)
        foreach(j RANGE ${next} ${spec_last})
            list(GET spec_order ${j} b)
            expect_compare("${a}" "${b}" -1)
        endforeach()
    endif()
endforeach()

# Sorting the example from a shuffled order by the comparison gives the order
# of the specification.
set(unsorted 1.0.0-beta.11 1.0.0 1.0.0-alpha.beta 1.0.0-rc.1 1.0.0-alpha
    1.0.0-beta.2 1.0.0-alpha.1 1.0.0-beta)
set(sorted "")
foreach(version IN LISTS unsorted)
    set(placed FALSE)
    set(new_sorted "")
    foreach(existing IN LISTS sorted)
        if(NOT placed)
            argx_semver_compare("${version}" "${existing}" result)
            if(result EQUAL -1)
                list(APPEND new_sorted "${version}")
                set(placed TRUE)
            endif()
        endif()
        list(APPEND new_sorted "${existing}")
    endforeach()
    if(NOT placed)
        list(APPEND new_sorted "${version}")
    endif()
    set(sorted "${new_sorted}")
endforeach()
argx_test_expect_equal("${sorted}" "${spec_order}" "sorting the example of the specification")

# Major, minor and patch compare as numbers, not as text.
expect_compare("1.0.0" "2.0.0" -1)
expect_compare("2.0.0" "2.1.0" -1)
expect_compare("2.1.0" "2.1.1" -1)
expect_compare("1.9.0" "1.10.0" -1)
expect_compare("0.0.9" "0.0.10" -1)
expect_compare("9.255.255" "10.0.0" -1)
expect_compare("0.1.0" "0.1.0" 0)

# The core decides before the pre-release identifiers do.
expect_compare("1.0.0" "1.0.1-alpha" -1)
expect_compare("1.0.0-rc.1" "1.0.1-alpha" -1)
expect_compare("0.1.0" "0.2.0-rc.1" -1)

# Numeric identifiers compare as numbers.
expect_compare("1.0.0-2" "1.0.0-10" -1)
expect_compare("1.0.0-rc.9" "1.0.0-rc.10" -1)
expect_compare("1.0.0-rc.0" "1.0.0-rc.1" -1)

# Long numeric identifiers compare as numbers too, without an overflow.
expect_compare("1.0.0-rc.99999999999999999999" "1.0.0-rc.100000000000000000000" -1)
expect_compare("1.0.0-rc.12345678901234567890" "1.0.0-rc.12345678901234567891" -1)
expect_compare("1.0.0-rc.9223372036854775807" "1.0.0-rc.9223372036854775808" -1)
expect_compare("1.0.0-rc.18446744073709551616" "1.0.0-rc.2" 1)
expect_compare("1.0.0-12345678901234567890" "1.0.0-12345678901234567890" 0)

# A numeric identifier is lower than an alphanumeric one.
expect_compare("1.0.0-1" "1.0.0-a" -1)
expect_compare("1.0.0-999" "1.0.0-0a" -1)
expect_compare("1.0.0-rc.99" "1.0.0-rc.a" -1)

# Alphanumeric identifiers compare in ASCII order.
expect_compare("1.0.0-B" "1.0.0-a" -1)
expect_compare("1.0.0--" "1.0.0-0a" -1)
expect_compare("1.0.0-alpha" "1.0.0-alpha-1" -1)
expect_compare("1.0.0-a10" "1.0.0-a9" -1)
expect_compare("1.0.0-rc" "1.0.0-rc" 0)

# With equal leading identifiers, more identifiers are higher.
expect_compare("1.0.0-rc" "1.0.0-rc.1" -1)
expect_compare("1.0.0-alpha.1" "1.0.0-alpha.1.0" -1)

# A pre-release is lower than the release of the same core.
expect_compare("1.0.0-0" "1.0.0" -1)
expect_compare("255.255.255-rc.1" "255.255.255" -1)

# --- Comparing an invalid version --------------------------------------------
#
# Each case runs argx_semver_compare in a child process, since a FATAL_ERROR
# would stop this script.

set(driver "${ARGX_TEST_WORK_DIR}/compare.cmake")
file(WRITE "${driver}" "
include(\"${ARGX_SOURCE_ROOT}/tools/release/ArgXSemver.cmake\")
argx_semver_compare(\"\${A}\" \"\${B}\" result)
message(\"compared: [\${result}]\")
")

function(expect_compare_rejected a b bad part)
    argx_test_run_script(run "${driver}" -D "A=${a}" -D "B=${b}")
    set(what "compare(${a}, ${b})")
    set(outcome "failed")
    if(run_RESULT EQUAL 0)
        set(outcome "succeeded")
    endif()
    argx_test_expect_equal("${outcome}" "failed" "${what}: the outcome")
    argx_test_expect_not_contains("${run_OUTPUT}" "compared: [" "${what}: no result")
    argx_test_expect_contains("${run_OUTPUT}" "error: " "${what}: an error")
    argx_test_expect_contains("${run_OUTPUT}" "\"${bad}\"" "${what}: the error names the version")
    # CMake wraps the text of an error into lines; join them again.
    string(REGEX REPLACE "[ \n]+" " " flat "${run_OUTPUT}")
    argx_test_expect_contains("${flat}" "${part}" "${what}: the reason")
endfunction()

expect_compare_rejected("1.2" "1.2.3" "1.2" "${form}")
expect_compare_rejected("1.2.3" "1.2.3+b" "1.2.3+b" "build metadata")
expect_compare_rejected("256.0.0" "1.0.0" "256.0.0" "${range}")

# The driver itself works, so the failures above come from the versions.
argx_test_run_script(run "${driver}" -D "A=1.0.0" -D "B=1.0.0-rc.1")
argx_test_expect_equal("${run_RESULT}" "0" "compare(1.0.0, 1.0.0-rc.1) in a child: the exit code")
argx_test_expect_contains("${run_OUTPUT}" "compared: [1]" "compare(1.0.0, 1.0.0-rc.1) in a child: the result")

argx_test_finish()
