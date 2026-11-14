# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# Tests of tools/release/publish.cmake: the script that checks the commit of a
# merged release pull request and makes the notes of its GitHub Release.
#
# Each case builds a small copy of a source tree in the work directory (the
# version header and CHANGELOG.md) and runs the script on it in a child
# process, as the publish workflow runs it, so that its exit code, its output,
# the notes file and the output file it appends to are checked as they are.
#
#   - A release and a pre-release whose header and changelog match the version
#     succeed: the notes file holds the section of the version from the
#     changelog, with a line ending at its end, and the output file gets
#     prerelease and title.
#   - The notes of a release that follows pre-releases of the same
#     <major>.<minor>.<patch> refer to them.
#   - The line endings of the changelog (LF or CRLF) are kept in the notes.
#   - A header or a changelog that does not match the version, and every other
#     check, fails with a non-zero exit code and the reason, and writes neither
#     the notes file nor the output file. So do notes that would be empty.
#   - The script writes nothing in the source tree.
#
# Run by CTest as release.publish; see ReleaseTestHelpers.cmake.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ReleaseTestHelpers.cmake")

set(script "${ARGX_SOURCE_ROOT}/tools/release/publish.cmake")
set(repo_url "https://github.com/example/project")

argx_test_copy_fixture(version.h header_fixture fixture-version.h)
file(READ "${header_fixture}" header_fixture_text)

# Sets <out-var> to the text of the header fixture, which holds 1.2.3, with
# the four values replaced by those of <major>.<minor>.<patch>[-<prerelease>].
function(header_text major minor patch prerelease out_var)
    set(text "${header_fixture_text}")
    string(REPLACE "#define ARGX_VERSION_MAJOR 1\n"
        "#define ARGX_VERSION_MAJOR ${major}\n" text "${text}")
    string(REPLACE "#define ARGX_VERSION_MINOR 2\n"
        "#define ARGX_VERSION_MINOR ${minor}\n" text "${text}")
    string(REPLACE "#define ARGX_VERSION_PATCH 3\n"
        "#define ARGX_VERSION_PATCH ${patch}\n" text "${text}")
    string(REPLACE "#define ARGX_VERSION_PRERELEASE \"\"\n"
        "#define ARGX_VERSION_PRERELEASE \"${prerelease}\"\n" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to the text of the fixture <fixture>, read as it is.
function(fixture_text fixture out_var)
    file(READ "${ARGX_TEST_FIXTURE_DIR}/${fixture}" text)
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to <text> with every LF turned into CRLF.
function(to_crlf text out_var)
    string(REPLACE "\n" "\r\n" text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to the bytes of <text> in hexadecimal, as file(READ HEX)
# gives them, so that two texts can be compared byte for byte.
function(text_hex text out_var)
    string(HEX "${text}" hex)
    set(${out_var} "${hex}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to the bytes of the file <path> in hexadecimal, or to
# "(no file)" when there is no such file.
function(file_hex path out_var)
    if(EXISTS "${path}")
        file(READ "${path}" hex HEX)
    else()
        set(hex "(no file)")
    endif()
    set(${out_var} "${hex}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to every file under <dir>, each with its bytes, so that a
# change of any file, and a file added or removed, changes the result.
function(tree_snapshot dir out_var)
    file(GLOB_RECURSE files RELATIVE "${dir}" "${dir}/*")
    list(SORT files)
    set(snapshot "")
    foreach(file IN LISTS files)
        file(READ "${dir}/${file}" hex HEX)
        string(APPEND snapshot "${file}=${hex}\n")
    endforeach()
    set(${out_var} "${snapshot}" PARENT_SCOPE)
endfunction()

# Makes the source tree <name> in the work directory, with <header> as
# include/argx/version.h and <changelog> as CHANGELOG.md. Sets <out-var> to
# the root of the tree.
function(make_tree name header changelog out_var)
    set(root "${ARGX_TEST_WORK_DIR}/${name}")
    argx_write_bytes("${root}/include/argx/version.h" "${header}" "the version header")
    argx_write_bytes("${root}/CHANGELOG.md" "${changelog}" "the changelog")
    set(${out_var} "${root}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to <text> with every run of blanks and line endings made one
# blank, since CMake wraps the text of a FATAL_ERROR over several lines.
function(flatten text out_var)
    string(REGEX REPLACE "[ \t\r\n]+" " " text "${text}")
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# Runs the script on the tree <root> with the version <version>, writing the
# notes to <root>.notes/notes.md (whose directory does not exist yet) and
# appending to the output file <root>.output, which starts as
# "earlier=value\n". Further arguments are passed to cmake before the script.
# Sets <prefix>_RESULT, <prefix>_OUTPUT (flattened), <prefix>_FILE (the
# content of the output file) and <prefix>_NOTES (the path of the notes file).
function(run_publish prefix root version)
    set(notes_path "${root}.notes/notes.md")
    set(output_path "${root}.output")
    file(WRITE "${output_path}" "earlier=value\n")
    argx_test_run_script(run "${script}"
        "-DARGX_RELEASE_VERSION=${version}"
        "-DARGX_RELEASE_REPO_URL=${repo_url}"
        "-DARGX_SOURCE_DIR=${root}"
        "-DARGX_RELEASE_NOTES=${notes_path}"
        "-DARGX_RELEASE_OUTPUT=${output_path}"
        ${ARGN})
    flatten("${run_OUTPUT}" output)
    file(READ "${output_path}" output_file)
    set(${prefix}_RESULT "${run_RESULT}" PARENT_SCOPE)
    set(${prefix}_OUTPUT "${output}" PARENT_SCOPE)
    set(${prefix}_FILE "${output_file}" PARENT_SCOPE)
    set(${prefix}_NOTES "${notes_path}" PARENT_SCOPE)
endfunction()

# Makes the tree <name> from <header> and <changelog>, runs the script with
# <version> and checks that it succeeds, that the notes file holds exactly
# <notes>, that the output file gets <output> after the line already there,
# and that the tree is not written.
function(expect_publish what name header changelog version notes output)
    make_tree(${name} "${header}" "${changelog}" root)
    tree_snapshot("${root}" before)
    run_publish(run "${root}" "${version}")
    argx_test_expect_equal("${run_RESULT}" "0" "${what}: the script succeeds")
    if(NOT "${run_RESULT}" STREQUAL "0")
        message("${what}: output of the script:\n${run_OUTPUT}")
    endif()
    text_hex("${notes}" expected_hex)
    file_hex("${run_NOTES}" actual_hex)
    argx_test_expect_equal("${actual_hex}" "${expected_hex}" "${what}: the notes, byte for byte")
    argx_test_expect_equal("${run_FILE}" "earlier=value\n${output}" "${what}: the output file")
    tree_snapshot("${root}" after)
    argx_test_expect_equal("${after}" "${before}" "${what}: the tree is not written")
    argx_test_expect_contains("${run_OUTPUT}" "v${version}" "${what}: the standard output names the tag")
endfunction()

header_text(0 1 0 "" header_0_1_0)
header_text(0 2 0 "rc.1" header_rc)
header_text(0 2 0 "" header_0_2_0)
fixture_text(CHANGELOG-first-release-expected.md first_changelog)
fixture_text(CHANGELOG-second-release-expected.md second_changelog)
fixture_text(CHANGELOG-publish.md publish_changelog)

# --- The first release -----------------------------------------------------------

string(CONCAT first_notes
    "### Added\n"
    "\n"
    "- A builder for command-line schemas, with options such as `--name=a;b`.\n"
    "- Parsing of `argv` into typed values\n"
    "  (a continuation line of the item above).\n"
    "  - A nested item, which is not counted as an entry.\n"
    "\n"
    "### Fixed\n"
    "\n"
    "* Paths such as `C:\\temp\\` and brackets such as `[x` are kept as they are.\n")
expect_publish("the first release" first "${header_0_1_0}" "${first_changelog}" 0.1.0
    "${first_notes}" "prerelease=false\ntitle=v0.1.0\n")

# The same with CRLF line endings: the notes keep them.
to_crlf("${header_0_1_0}" header_0_1_0_crlf)
to_crlf("${first_changelog}" first_changelog_crlf)
to_crlf("${first_notes}" first_notes_crlf)
expect_publish("the first release with CRLF" first_crlf "${header_0_1_0_crlf}"
    "${first_changelog_crlf}" 0.1.0 "${first_notes_crlf}" "prerelease=false\ntitle=v0.1.0\n")

# --- A pre-release ------------------------------------------------------------------

string(CONCAT rc_notes
    "### Changed\n"
    "\n"
    "- The help text wraps at 80 columns.\n"
    "\n"
    "### Security\n"
    "\n"
    "- Option values are checked for length.\n")
expect_publish("a pre-release" pre "${header_rc}" "${second_changelog}" 0.2.0-rc.1
    "${rc_notes}" "prerelease=true\ntitle=v0.2.0-rc.1\n")

# --- A release after pre-releases ------------------------------------------------------

string(CONCAT release_notes
    "### Fixed\n"
    "\n"
    "- Option values with `;` are kept as they are.\n"
    "\n"
    "This release also includes the changes in the pre-releases "
    "[v0.2.0-rc.2](https://github.com/example/project/releases/tag/v0.2.0-rc.2), "
    "[v0.2.0-rc.1](https://github.com/example/project/releases/tag/v0.2.0-rc.1).\n")
expect_publish("a release after pre-releases" release "${header_0_2_0}" "${publish_changelog}"
    0.2.0 "${release_notes}" "prerelease=false\ntitle=v0.2.0\n")

# Without an output file, the script still writes the notes.
make_tree(no_output "${header_0_2_0}" "${publish_changelog}" root)
argx_test_run_script(run "${script}"
    "-DARGX_RELEASE_VERSION=0.2.0"
    "-DARGX_RELEASE_REPO_URL=${repo_url}"
    "-DARGX_SOURCE_DIR=${root}"
    "-DARGX_RELEASE_NOTES=${root}.notes.md")
argx_test_expect_equal("${run_RESULT}" "0" "without an output file: the script succeeds")
text_hex("${release_notes}" expected_hex)
file_hex("${root}.notes.md" actual_hex)
argx_test_expect_equal("${actual_hex}" "${expected_hex}" "without an output file: the notes")

# --- Failures -------------------------------------------------------------------------

# Makes the tree <name> from <header> and <changelog>, runs the script with
# <version> and checks that it fails with "error: " and <reason> in its
# output, that it writes neither the notes file nor the output file, and that
# the tree has not changed.
function(expect_failure what name header changelog version reason)
    make_tree(${name} "${header}" "${changelog}" root)
    tree_snapshot("${root}" before)
    run_publish(run "${root}" "${version}" ${ARGN})
    if("${run_RESULT}" STREQUAL "0")
        argx_test_expect_equal("${run_RESULT}" "non-zero" "${what}: the script fails")
    else()
        argx_test_expect_equal("failed" "failed" "${what}: the script fails")
    endif()
    argx_test_expect_contains("${run_OUTPUT}" "error: " "${what}: the output starts the reason with error:")
    argx_test_expect_contains("${run_OUTPUT}" "${reason}" "${what}: the reason")
    file_hex("${run_NOTES}" notes_hex)
    argx_test_expect_equal("${notes_hex}" "(no file)" "${what}: the notes file is not written")
    argx_test_expect_equal("${run_FILE}" "earlier=value\n" "${what}: the output file is not written")
    tree_snapshot("${root}" after)
    argx_test_expect_equal("${after}" "${before}" "${what}: the tree is not written")
endfunction()

# Missing inputs.
set(index 0)
foreach(input IN ITEMS ARGX_RELEASE_VERSION ARGX_RELEASE_REPO_URL ARGX_SOURCE_DIR
        ARGX_RELEASE_NOTES)
    math(EXPR index "${index} + 1")
    expect_failure("${input} is empty" missing_${index} "${header_0_2_0}" "${publish_changelog}"
        0.2.0 "${input} is not set" "-D${input}=")
endforeach()
expect_failure("the source tree does not exist" no_tree "${header_0_2_0}" "${publish_changelog}"
    0.2.0 "does not exist" "-DARGX_SOURCE_DIR=${ARGX_TEST_WORK_DIR}/none")

# Versions that are not valid, such as the name of the tag or of the branch.
foreach(version IN ITEMS 0.2 v0.2.0 release/v0.2.0 0.2.0+build 01.2.0 256.0.0)
    string(MAKE_C_IDENTIFIER "${version}" id)
    expect_failure("the version ${version}" version_${id} "${header_0_2_0}" "${publish_changelog}"
        "${version}" "the version to publish is not valid")
endforeach()

# The header does not hold the version: another number, or the same numbers
# with or without a pre-release.
header_text(0 2 1 "" header_0_2_1)
expect_failure("the header holds another version" header_other "${header_0_2_1}"
    "${publish_changelog}" 0.2.0
    "the version header holds 0.2.1, not the version to publish 0.2.0")
expect_failure("the header holds a pre-release" header_pre "${header_rc}"
    "${publish_changelog}" 0.2.0
    "the version header holds 0.2.0-rc.1, not the version to publish 0.2.0")
expect_failure("the header holds the release" header_release "${header_0_2_0}"
    "${second_changelog}" 0.2.0-rc.1
    "the version header holds 0.2.0, not the version to publish 0.2.0-rc.1")

# The newest version of the changelog is not the version: it is not listed,
# or it is listed below a newer one.
expect_failure("the changelog does not list the version" changelog_missing "${header_0_2_0}"
    "${second_changelog}" 0.2.0
    "the newest version in the changelog is 0.2.0-rc.1, not the version to publish 0.2.0")
expect_failure("the changelog lists a newer version" changelog_newer "${header_rc}"
    "${publish_changelog}" 0.2.0-rc.1
    "the newest version in the changelog is 0.2.0, not the version to publish 0.2.0-rc.1")
expect_failure("the changelog lists no version" changelog_none "${header_0_1_0}"
    "# Changelog\n\n## [Unreleased]\n\n### Added\n\n- An entry.\n\n[Unreleased]: ${repo_url}/commits/main\n"
    0.1.0 "the changelog lists no version")

# The changelog is not of the accepted form.
fixture_text(CHANGELOG-invalid.md invalid_changelog)
expect_failure("an invalid changelog" changelog_invalid "${header_0_1_0}" "${invalid_changelog}"
    0.1.0 "CHANGELOG.md is not valid")

# The section of the version is empty, so the notes would be empty.
string(CONCAT empty_changelog
    "# Changelog\n"
    "\n"
    "## [Unreleased]\n"
    "\n"
    "## [0.1.0] - 2026-11-13\n"
    "\n"
    "\n"
    "[Unreleased]: ${repo_url}/compare/v0.1.0...HEAD\n"
    "[0.1.0]: ${repo_url}/releases/tag/v0.1.0\n")
expect_failure("empty notes" notes_empty "${header_0_1_0}" "${empty_changelog}" 0.1.0
    "the notes of v0.1.0 are empty")

argx_test_finish()
