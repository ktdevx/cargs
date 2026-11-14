# Tests of cline_read_version (cmake/ClineVersion.cmake): reading the
# pre-release identifiers, and stopping on a header whose pre-release macro is
# missing, defined twice or not a valid value.
#
# Run by CTest as release.version_read; see ReleaseTestHelpers.cmake.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ReleaseTestHelpers.cmake")
include("${CLINE_SOURCE_ROOT}/cmake/ClineVersion.cmake")

set(release_line "#define CLINE_VERSION_PRERELEASE \"\"")

cline_test_copy_fixture(version.h fixture)
file(READ "${fixture}" fixture_text)

# Writes the fixture to <name> in the work directory with the pre-release line
# replaced by <line> (removed when <line> is empty), and sets <out-var> to the
# path.
function(write_header name line out_var)
    string(REPLACE "${release_line}" "${line}" text "${fixture_text}")
    set(path "${CLINE_TEST_WORK_DIR}/${name}")
    file(WRITE "${path}" "${text}")
    set(${out_var} "${path}" PARENT_SCOPE)
endfunction()

# The fixture really holds the line that the cases below replace.
cline_test_expect_contains("${fixture_text}" "${release_line}"
    "the fixture holds the release pre-release line")

# --- Valid headers -----------------------------------------------------------

# A release: the identifiers are empty.
cline_read_version("${fixture}" version prerelease)
cline_test_expect_equal("${version}" "1.2.3" "release: the version")
cline_test_expect_equal("${prerelease}" "" "release: the pre-release identifiers")

# A third argument names a variable that is set even when it held a value.
set(prerelease "stale")
cline_read_version("${fixture}" version prerelease)
cline_test_expect_equal("${prerelease}" "" "release: a stale variable is cleared")

foreach(id IN ITEMS "rc.1" "alpha.beta.1" "0" "rc.0" "01a" "x-y.7" "alpha-1.2.3")
    write_header("valid-${id}.h" "#define CLINE_VERSION_PRERELEASE \"${id}\"" path)
    cline_read_version("${path}" version prerelease)
    cline_test_expect_equal("${version}" "1.2.3" "\"${id}\": the version")
    cline_test_expect_equal("${prerelease}" "${id}" "\"${id}\": the pre-release identifiers")

    # Without the third argument, the result is the numeric version alone, as
    # before the pre-release macro existed.
    cline_read_version("${path}" version_only)
    cline_test_expect_equal("${version_only}" "1.2.3"
        "\"${id}\": the version read with two arguments")
endforeach()

# Blanks around the tokens are ignored, as the compiler ignores them.
write_header("valid-blanks.h" "  #  define\tCLINE_VERSION_PRERELEASE   \"rc.2\"  " path)
cline_read_version("${path}" version prerelease)
cline_test_expect_equal("${prerelease}" "rc.2" "blanks: the pre-release identifiers")

# A call with two arguments returns what it returned before: the numeric
# version of the header of this source tree, the same as with three.
set(source_header "${CLINE_SOURCE_ROOT}/include/cline/version.h")
cline_read_version("${source_header}" source_version)
cline_read_version("${source_header}" source_version_3 source_prerelease)
cline_test_expect_equal("${source_version}" "${source_version_3}"
    "source header: two and three arguments give the same version")
file(STRINGS "${source_header}" source_numbers
    REGEX "^#define CLINE_VERSION_(MAJOR|MINOR|PATCH) ")
string(REGEX REPLACE "#define CLINE_VERSION_[A-Z]+ " "" source_numbers "${source_numbers}")
string(REPLACE ";" "." source_numbers "${source_numbers}")
cline_test_expect_equal("${source_version}" "${source_numbers}"
    "source header: the version is the three numeric macros")

# --- Invalid headers ---------------------------------------------------------
#
# Each case runs cline_read_version in a child process, since a FATAL_ERROR
# would stop this script. The driver reads HEADER with two or three arguments.

set(driver "${CLINE_TEST_WORK_DIR}/read_version.cmake")
file(WRITE "${driver}" "
include(\"${CLINE_SOURCE_ROOT}/cmake/ClineVersion.cmake\")
if(ARGUMENTS EQUAL 3)
    cline_read_version(\"\${HEADER}\" version prerelease)
else()
    cline_read_version(\"\${HEADER}\" version)
endif()
message(\"read: [\${version}] [\${prerelease}]\")
")

# Reads <path> with two and with three arguments. Both must fail with one
# error that names the header and holds each of the <part>s.
function(expect_rejected case path)
    foreach(arguments IN ITEMS 2 3)
        cline_test_run_script(run "${driver}"
            -D "HEADER=${path}" -D "ARGUMENTS=${arguments}")
        set(what "${case} (${arguments} arguments)")
        set(outcome "failed")
        if(run_RESULT EQUAL 0)
            set(outcome "succeeded")
        endif()
        cline_test_expect_equal("${outcome}" "failed" "${what}: the outcome")
        cline_test_expect_not_contains("${run_OUTPUT}" "read: [" "${what}: nothing is read")
        cline_test_expect_contains("${run_OUTPUT}" "${path}" "${what}: the error names the header")
        foreach(part IN LISTS ARGN)
            cline_test_expect_contains("${run_OUTPUT}" "${part}" "${what}: the error")
        endforeach()
        # All the problems are in one error.
        string(REGEX MATCHALL "cannot read the version from" heads "${run_OUTPUT}")
        list(LENGTH heads count)
        cline_test_expect_equal("${count}" "1" "${what}: one error")
    endforeach()
endfunction()

# The header of the three numeric macros alone, without the fourth.
write_header("missing.h" "" path)
expect_rejected("missing" "${path}"
    "CLINE_VERSION_PRERELEASE was not found")

# The macro only inside a comment does not count.
write_header("comment-only.h" "/* ${release_line} */" path)
expect_rejected("only in a comment" "${path}"
    "CLINE_VERSION_PRERELEASE was not found")

write_header("twice.h" "${release_line}\n#define CLINE_VERSION_PRERELEASE \"rc.1\"" path)
expect_rejected("defined twice" "${path}"
    "CLINE_VERSION_PRERELEASE is defined more than once")

foreach(id IN ITEMS "01" "rc..1" "rc_1" ".rc" "rc." "rc.01" "rc 1" "rc+1")
    string(MAKE_C_IDENTIFIER "${id}" file_name)
    write_header("invalid-${file_name}.h" "#define CLINE_VERSION_PRERELEASE \"${id}\"" path)
    expect_rejected("\"${id}\"" "${path}"
        "CLINE_VERSION_PRERELEASE is \"${id}\", which are not valid pre-release identifiers")
endforeach()

foreach(value IN ITEMS "rc.1" "0" "'rc'" "\"rc.1" "\"rc\" \"1\"" "L\"rc\"")
    string(MAKE_C_IDENTIFIER "${value}" file_name)
    write_header("not-string-${file_name}.h" "#define CLINE_VERSION_PRERELEASE ${value}" path)
    expect_rejected("${value}" "${path}"
        "CLINE_VERSION_PRERELEASE is ${value}, which is not a string literal")
endforeach()

write_header("no-value.h" "#define CLINE_VERSION_PRERELEASE" path)
expect_rejected("no value" "${path}"
    "CLINE_VERSION_PRERELEASE has no value")

# A problem of a numeric macro and of the pre-release macro are in one error.
string(REPLACE "#define CLINE_VERSION_MAJOR 1" "#define CLINE_VERSION_MAJOR 256"
    major_text "${fixture_text}")
string(REPLACE "${release_line}" "#define CLINE_VERSION_PRERELEASE \"rc_1\"" both_text "${major_text}")
file(WRITE "${CLINE_TEST_WORK_DIR}/both.h" "${both_text}")
expect_rejected("numeric and pre-release" "${CLINE_TEST_WORK_DIR}/both.h"
    "CLINE_VERSION_MAJOR is \"256\""
    "CLINE_VERSION_PRERELEASE is \"rc_1\"")

cline_test_finish()
