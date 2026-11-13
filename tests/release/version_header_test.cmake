# Tests of tools/release/ClargsVersionHeader.cmake: rewriting the four version
# macros of a version header to a release or a pre-release keeps every other
# byte, and a header whose macro line is not found exactly once, or a version
# that is not valid, stops the rewrite with the reason.
#
# Run by CTest as release.version_header; see ReleaseTestHelpers.cmake.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ReleaseTestHelpers.cmake")
include("${CLARGS_SOURCE_ROOT}/cmake/ClargsVersion.cmake")
include("${CLARGS_SOURCE_ROOT}/tools/release/ClargsVersionHeader.cmake")

clargs_test_copy_fixture(version.h fixture)
file(READ "${fixture}" fixture_text)

# The four value lines of the fixture, which hold the version 1.2.3.
set(major_line "#define CLARGS_VERSION_MAJOR 1")
set(minor_line "#define CLARGS_VERSION_MINOR 2")
set(patch_line "#define CLARGS_VERSION_PATCH 3")
set(prerelease_line "#define CLARGS_VERSION_PRERELEASE \"\"")
foreach(line IN ITEMS major_line minor_line patch_line prerelease_line)
    clargs_test_expect_contains("${fixture_text}" "${${line}}" "the fixture holds ${line}")
endforeach()

# Sets <out-var> to <text> with the four value lines of the fixture replaced
# by the lines of the version <major>.<minor>.<patch>[-<prerelease>], which is
# what the rewrite must produce: nothing else may change.
function(expected_text text major minor patch prerelease out_var)
    string(REPLACE "${major_line}" "#define CLARGS_VERSION_MAJOR ${major}" text "${text}")
    string(REPLACE "${minor_line}" "#define CLARGS_VERSION_MINOR ${minor}" text "${text}")
    string(REPLACE "${patch_line}" "#define CLARGS_VERSION_PATCH ${patch}" text "${text}")
    string(REPLACE "${prerelease_line}" "#define CLARGS_VERSION_PRERELEASE \"${prerelease}\""
        text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# Rewrites <text> to the version, checks the result against expected_text(),
# writes it to <name> in the work directory and checks that clargs_read_version
# reads the version back.
function(expect_render what text major minor patch prerelease)
    clargs_version_header_render("${text}" "${major}" "${minor}" "${patch}" "${prerelease}" result)
    expected_text("${text}" "${major}" "${minor}" "${patch}" "${prerelease}" expected)
    clargs_test_expect_equal("${result}" "${expected}" "${what}: only the four values change")

    string(MAKE_C_IDENTIFIER "${what}" name)
    set(path "${CLARGS_TEST_WORK_DIR}/${name}.h")
    if(result MATCHES "\n$")
        clargs_write_bytes("${path}" "${result}" "the header")
    else()
        # On Windows, a text without a line ending at its end cannot be written
        # byte for byte, and file(WRITE) changes only its line endings there,
        # which clargs_read_version() does not mind.
        file(WRITE "${path}" "${result}")
    endif()
    clargs_read_version("${path}" version read_prerelease)
    clargs_test_expect_equal("${version}" "${major}.${minor}.${patch}" "${what}: the version read back")
    clargs_test_expect_equal("${read_prerelease}" "${prerelease}"
        "${what}: the pre-release identifiers read back")
endfunction()

# --- Rewrites ----------------------------------------------------------------

expect_render("release to pre-release" "${fixture_text}" 0 2 0 "rc.1")
expect_render("release to release" "${fixture_text}" 10 255 0 "")
expect_render("release to a long pre-release" "${fixture_text}" 2 0 0 "alpha-1.beta.0.x-y")

# A pre-release back to the release it came from gives the fixture again.
clargs_version_header_render("${fixture_text}" 0 2 0 "rc.1" prerelease_text)
clargs_version_header_render("${prerelease_text}" 1 2 3 "" round_trip)
clargs_test_expect_equal("${round_trip}" "${fixture_text}" "pre-release to release: the fixture again")

# The macros inside the comment of the fixture are not values to rewrite.
clargs_test_expect_contains("${prerelease_text}" " * #define CLARGS_VERSION_MAJOR 9\n"
    "the macro in a comment keeps its value")
clargs_test_expect_contains("${prerelease_text}" " * #define CLARGS_VERSION_PRERELEASE \"comment.1\"\n"
    "the pre-release macro in a comment keeps its value")

# CRLF line endings stay CRLF, and no LF appears on its own.
string(REPLACE "\n" "\r\n" crlf_text "${fixture_text}")
expect_render("CRLF" "${crlf_text}" 0 3 0 "beta.2")
clargs_version_header_render("${crlf_text}" 0 3 0 "beta.2" crlf_result)
string(REPLACE "\r\n" "" crlf_rest "${crlf_result}")
clargs_test_expect_not_contains("${crlf_rest}" "\n" "CRLF: every LF follows a CR")

# A header without a newline at its end gets none.
string(REGEX REPLACE "\n+$" "" no_newline_text "${fixture_text}")
expect_render("no newline at the end" "${no_newline_text}" 0 3 0 "")

# A header whose last line is a value line.
string(REGEX REPLACE "\n\n#endif[^\n]*\n$" "" last_line_text "${fixture_text}")
clargs_test_expect_equal("${CMAKE_MATCH_0}" "\n\n#endif /* CLARGS_VERSION_H */\n"
    "the fixture ends with #endif")
expect_render("value on the last line" "${last_line_text}" 0 3 1 "rc.3")

# Blanks around the tokens and a comment after the value stay as they are.
string(REPLACE "${minor_line}" "  #  define\tCLARGS_VERSION_MINOR   2  /* minor */" blanks_text
    "${fixture_text}")
string(REPLACE "${prerelease_line}" "#define CLARGS_VERSION_PRERELEASE\t\"\" // none"
    blanks_text "${blanks_text}")
clargs_version_header_render("${blanks_text}" 0 7 0 "rc.1" blanks_result)
string(REPLACE "  #  define\tCLARGS_VERSION_MINOR   2  /* minor */"
    "  #  define\tCLARGS_VERSION_MINOR   7  /* minor */" blanks_expected "${blanks_text}")
string(REPLACE "#define CLARGS_VERSION_PRERELEASE\t\"\" // none"
    "#define CLARGS_VERSION_PRERELEASE\t\"rc.1\" // none" blanks_expected "${blanks_expected}")
string(REPLACE "${major_line}" "#define CLARGS_VERSION_MAJOR 0" blanks_expected "${blanks_expected}")
string(REPLACE "${patch_line}" "#define CLARGS_VERSION_PATCH 0" blanks_expected "${blanks_expected}")
clargs_test_expect_equal("${blanks_result}" "${blanks_expected}"
    "blanks and comments: only the values change")

# A macro that a block comment of several lines hides does not count.
string(REPLACE "#endif /* CLARGS_VERSION_H */"
    "/*\n#define CLARGS_VERSION_MAJOR 7\n*/\n#endif /* CLARGS_VERSION_H */" hidden_text
    "${fixture_text}")
expect_render("macro in a block comment" "${hidden_text}" 0 4 0 "")
clargs_version_header_render("${hidden_text}" 0 4 0 "" hidden_result)
clargs_test_expect_contains("${hidden_result}" "/*\n#define CLARGS_VERSION_MAJOR 7\n*/"
    "the macro in a block comment keeps its value")

# Semicolons and brackets elsewhere in the text do not disturb the rewrite.
string(REPLACE "#ifndef CLARGS_VERSION_H"
    "/* a; b [c] \\d */\n#ifndef CLARGS_VERSION_H" odd_text "${fixture_text}")
expect_render("semicolons and brackets" "${odd_text}" 0 5 0 "rc.1")

# The header of this source tree can be rewritten: rewriting it to its own
# version gives it back unchanged, and to another version reads back. Only the
# text is rewritten here; the file itself is not touched.
set(source_header "${CLARGS_SOURCE_ROOT}/include/clargs/version.h")
file(READ "${source_header}" source_text)
clargs_read_version("${source_header}" source_version source_prerelease)
string(REPLACE "." ";" source_numbers "${source_version}")
list(GET source_numbers 0 source_major)
list(GET source_numbers 1 source_minor)
list(GET source_numbers 2 source_patch)
clargs_version_header_render("${source_text}" ${source_major} ${source_minor} ${source_patch}
    "${source_prerelease}" source_same)
clargs_test_expect_equal("${source_same}" "${source_text}" "source header: its own version")
clargs_version_header_render("${source_text}" 9 8 7 "rc.6" source_other)
set(path "${CLARGS_TEST_WORK_DIR}/source-other.h")
clargs_write_bytes("${path}" "${source_other}" "the header")
clargs_read_version("${path}" version read_prerelease)
clargs_test_expect_equal("${version}-${read_prerelease}" "9.8.7-rc.6"
    "source header: another version read back")

# --- Writing a header file ---------------------------------------------------

clargs_test_copy_fixture(version.h header_copy "write/version.h")
clargs_version_header_write("${header_copy}" 0 2 0 "rc.1")
file(READ "${header_copy}" written)
expected_text("${fixture_text}" 0 2 0 "rc.1" written_expected)
clargs_test_expect_equal("${written}" "${written_expected}" "write: the file holds the new values")
clargs_read_version("${header_copy}" written_version written_prerelease)
clargs_test_expect_equal("${written_version}-${written_prerelease}" "0.2.0-rc.1"
    "write: the version read back")

# CRLF line endings stay CRLF in the file too. file(READ) drops the CR of a
# CRLF, so the bytes are compared in hexadecimal.
set(crlf_copy "${CLARGS_TEST_WORK_DIR}/write/crlf.h")
clargs_write_bytes("${crlf_copy}" "${crlf_text}" "the header")
file(READ "${crlf_copy}" crlf_hex HEX)
clargs_test_expect_contains("${crlf_hex}" "0d0a" "write CRLF: the input holds CRLF")
clargs_version_header_write("${crlf_copy}" 0 3 0 "")
file(READ "${crlf_copy}" written_crlf HEX)
expected_text("${crlf_text}" 0 3 0 "" written_crlf_expected)
string(HEX "${written_crlf_expected}" written_crlf_expected)
clargs_test_expect_equal("${written_crlf}" "${written_crlf_expected}" "write CRLF: the bytes of the file")

# --- Failures ----------------------------------------------------------------
#
# Each case runs the rewrite in a child process, since a FATAL_ERROR would stop
# this script. The header is passed in a file.

set(driver "${CLARGS_TEST_WORK_DIR}/driver.cmake")
file(WRITE "${driver}" "
include(\"${CLARGS_SOURCE_ROOT}/tools/release/ClargsVersionHeader.cmake\")
if(ACTION STREQUAL \"write\")
    clargs_version_header_write(\"\${HEADER}\" \"\${MAJOR}\" \"\${MINOR}\" \"\${PATCH}\" \"\${PRERELEASE}\")
    message(\"done: [written]\")
else()
    file(READ \"\${HEADER}\" text)
    clargs_version_header_render(\"\${text}\" \"\${MAJOR}\" \"\${MINOR}\" \"\${PATCH}\" \"\${PRERELEASE}\" result)
    message(\"done: [\${result}]\")
endif()
")

set(case_number 0)

# Rewrites <text> with the -D arguments that follow, and checks that it stops
# with an error that holds <part>.
function(expect_rejected what text part)
    math(EXPR number "${case_number} + 1")
    set(case_number ${number} PARENT_SCOPE)
    set(header "${CLARGS_TEST_WORK_DIR}/case-${number}.h")
    clargs_write_bytes("${header}" "${text}" "the header")
    clargs_test_run_script(run "${driver}" -D "HEADER=${header}" ${ARGN})
    set(outcome "failed")
    if(run_RESULT EQUAL 0)
        set(outcome "succeeded")
    endif()
    clargs_test_expect_equal("${outcome}" "failed" "${what}: the outcome")
    clargs_test_expect_not_contains("${run_OUTPUT}" "done: [" "${what}: no result")
    clargs_test_expect_contains("${run_OUTPUT}" "error: " "${what}: an error")
    # CMake wraps the text of an error into lines; join them again.
    string(REGEX REPLACE "[ \n]+" " " flat "${run_OUTPUT}")
    clargs_test_expect_contains("${flat}" "${part}" "${what}: the reason")
    # A failed write leaves the file as it was.
    file(READ "${header}" after)
    clargs_test_expect_equal("${after}" "${text}" "${what}: the header is unchanged")
endfunction()

set(to_rc -D MAJOR=0 -D MINOR=2 -D PATCH=0 -D PRERELEASE=rc.1)

foreach(action IN ITEMS render write)
    set(args -D ACTION=${action} ${to_rc})

    string(REPLACE "${minor_line}\n" "" text "${fixture_text}")
    expect_rejected("${action}: a missing macro" "${text}"
        "CLARGS_VERSION_MINOR was not found" ${args})

    string(REPLACE "${patch_line}" "${patch_line}\n${patch_line}" text "${fixture_text}")
    expect_rejected("${action}: a macro defined twice" "${text}"
        "CLARGS_VERSION_PATCH is defined more than once" ${args})

    string(REPLACE "${prerelease_line}" "// ${prerelease_line}" text "${fixture_text}")
    expect_rejected("${action}: a macro only in a comment" "${text}"
        "CLARGS_VERSION_PRERELEASE was not found" ${args})

    string(REPLACE "${major_line}" "#define CLARGS_VERSION_MAJOR" text "${fixture_text}")
    expect_rejected("${action}: a macro without a value" "${text}"
        "CLARGS_VERSION_MAJOR has no value" ${args})

    string(REPLACE "${prerelease_line}" "#define CLARGS_VERSION_PRERELEASE rc1" text
        "${fixture_text}")
    expect_rejected("${action}: a pre-release that is not a string literal" "${text}"
        "CLARGS_VERSION_PRERELEASE is rc1, which is not a string literal" ${args})

    # Every problem is reported, not only the first one.
    string(REPLACE "${major_line}\n" "" text "${fixture_text}")
    string(REPLACE "${patch_line}" "${patch_line}\n${patch_line}" text "${text}")
    expect_rejected("${action}: two problems (MAJOR)" "${text}"
        "CLARGS_VERSION_MAJOR was not found" ${args})
    expect_rejected("${action}: two problems (PATCH)" "${text}"
        "CLARGS_VERSION_PATCH is defined more than once" ${args})

    # The version itself must be valid.
    expect_rejected("${action}: a number above 255" "${fixture_text}"
        "\"256.2.0-rc.1\" is not a valid version"
        -D ACTION=${action} -D MAJOR=256 -D MINOR=2 -D PATCH=0 -D PRERELEASE=rc.1)
    expect_rejected("${action}: a leading zero" "${fixture_text}"
        "\"0.02.0\" is not a valid version"
        -D ACTION=${action} -D MAJOR=0 -D MINOR=02 -D PATCH=0 -D PRERELEASE=)
    expect_rejected("${action}: an empty number" "${fixture_text}"
        "the minor version \"\" is not a number"
        -D ACTION=${action} -D MAJOR=0 -D MINOR= -D PATCH=0 -D PRERELEASE=)
    expect_rejected("${action}: an empty identifier" "${fixture_text}"
        "\"0.2.0-rc..1\" is not a valid version"
        -D ACTION=${action} -D MAJOR=0 -D MINOR=2 -D PATCH=0 -D PRERELEASE=rc..1)
    expect_rejected("${action}: a number that is a version" "${fixture_text}"
        "the major version \"1.2\" is not a number"
        -D ACTION=${action} -D MAJOR=1.2 -D MINOR=0 -D PATCH=0 -D PRERELEASE=)
endforeach()

expect_rejected("write: a missing header" "" "does not exist"
    -D ACTION=write ${to_rc} -D "HEADER=${CLARGS_TEST_WORK_DIR}/missing/version.h")

# The driver itself works, so the failures above come from the cases.
set(header "${CLARGS_TEST_WORK_DIR}/driver-check.h")
clargs_write_bytes("${header}" "${fixture_text}" "the header")
clargs_test_run_script(run "${driver}" -D "HEADER=${header}" -D ACTION=render ${to_rc})
clargs_test_expect_equal("${run_RESULT}" "0" "render in a child: the exit code")
clargs_test_expect_contains("${run_OUTPUT}" "done: [/*" "render in a child: the result")
clargs_test_run_script(run "${driver}" -D "HEADER=${header}" -D ACTION=write ${to_rc})
clargs_test_expect_equal("${run_RESULT}" "0" "write in a child: the exit code")
clargs_test_expect_contains("${run_OUTPUT}" "done: [written]" "write in a child: the result")

clargs_test_finish()
