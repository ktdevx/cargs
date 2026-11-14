# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# Tests of tools/ci/changes.cmake: a list of changed files is documentation
# only when every path in it is documentation, and a list that is missing,
# empty or holds a path that is not documentation runs the whole CI. Markdown
# files count as documentation only at the root of the repository, under
# docs/ and under .github/; the Markdown fixtures under tests/ are inputs of
# the tests and do not.
#
# The script runs in a child process, as the CI runs it, so that its exit
# code, its standard output and the file it appends to are checked as they
# are.
#
# Run by CTest as release.changes; see ReleaseTestHelpers.cmake.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ReleaseTestHelpers.cmake")

set(script "${ARGX_SOURCE_ROOT}/tools/ci/changes.cmake")

# Runs the script with the given "-D" arguments. Sets <prefix>_RESULT to its
# exit code, <prefix>_STDOUT to its standard output only and <prefix>_STDERR
# to its standard error.
function(run_changes prefix)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${ARGN} -P "${script}"
        WORKING_DIRECTORY "${ARGX_TEST_WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    set(${prefix}_RESULT "${result}" PARENT_SCOPE)
    set(${prefix}_STDOUT "${output}" PARENT_SCOPE)
    set(${prefix}_STDERR "${error}" PARENT_SCOPE)
endfunction()

# Sets <out-var> to the content of <path>, or to "(no file)" when there is no
# such file, so that a missing output file fails a check instead of the test.
function(read_output path out_var)
    if(EXISTS "${path}")
        file(READ "${path}" text)
    else()
        set(text "(no file)")
    endif()
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

# Writes <content> as the list <name> in the work directory, runs the script
# on it with an output file and checks that it succeeds, prints
# "docs_only=<expected>" on its standard output and appends exactly
# "docs_only=<expected>" and a newline to the output file. Sets
# <prefix>_STDOUT to the standard output of the script.
function(expect_docs_only prefix what name content expected)
    set(list_path "${ARGX_TEST_WORK_DIR}/${name}.txt")
    set(output_path "${ARGX_TEST_WORK_DIR}/${name}.output")
    file(WRITE "${list_path}" "${content}")
    file(WRITE "${output_path}" "earlier=value\n")
    run_changes(run
        "-DARGX_CHANGED_FILES=${list_path}"
        "-DARGX_CHANGES_OUTPUT=${output_path}")
    argx_test_expect_equal("${run_RESULT}" "0" "${what}: the script succeeds")
    argx_test_expect_contains("${run_STDOUT}" "docs_only=${expected}"
        "${what}: the verdict on the standard output")
    read_output("${output_path}" output)
    argx_test_expect_equal("${output}" "earlier=value\ndocs_only=${expected}\n"
        "${what}: the verdict appended to the output file")
    set(${prefix}_STDOUT "${run_STDOUT}" PARENT_SCOPE)
endfunction()

# --- Documentation only ------------------------------------------------------

string(CONCAT all_list
    "README.md\nCHANGELOG.md\nCONTRIBUTING.md\ndocs/versioning.md\ndocs/releasing.md\n"
    "docs/images/diagram.png\nLICENSE\n.github/pull_request_template.md\n"
    ".github/ISSUE_TEMPLATE/bug_report.yml\n.github/ISSUE_TEMPLATE/config.yml\n")
expect_docs_only(all "every kind of documentation" all "${all_list}" "true")
expect_docs_only(markdown "Markdown at the root, under docs/ and under .github/" markdown
    "CHANGELOG.md\ndocs/conventions.md\n.github/SUPPORT.md\n" "true")
expect_docs_only(docs "files under docs/ only" docs
    "docs/releasing.md\ndocs/images/logo.svg\ndocs/data/example.json\n" "true")
expect_docs_only(image "an image under docs/" image "docs/images/diagram.png\n" "true")
expect_docs_only(one "a single path without a final newline" one "README.md" "true")
expect_docs_only(blank "blank lines between the paths" blank "\nREADME.md\n\n\nLICENSE\n\n" "true")
expect_docs_only(crlf "CRLF line endings" crlf "README.md\r\ndocs/versioning.md\r\nLICENSE\r\n" "true")
expect_docs_only(semicolon "a path with a semicolon under docs/" semicolon
    "docs/a;b.png\nREADME.md\n" "true")
argx_test_expect_contains("${all_STDOUT}" "(10 in all)" "the number of paths is printed")

# --- Not documentation only --------------------------------------------------

expect_docs_only(mixed "documentation and a source file" mixed
    "README.md\nsrc/argx.c\ninclude/argx/argx.h\ndocs/versioning.md\n" "false")
argx_test_expect_contains("${mixed_STDOUT}" "src/argx.c"
    "mixed: the first path that is not documentation is printed")
argx_test_expect_not_contains("${mixed_STDOUT}" "include/argx/argx.h"
    "mixed: only the first path that is not documentation is printed")

expect_docs_only(workflow "the CI workflow" workflow
    "README.md\n.github/workflows/ci.yml\n" "false")
argx_test_expect_contains("${workflow_STDOUT}" ".github/workflows/ci.yml"
    "workflow: the workflow is the path that is not documentation")

expect_docs_only(fixtures "Markdown fixtures under tests/ only" fixtures
    "tests/release/fixtures/CHANGELOG-valid.md\ntests/release/fixtures/CHANGELOG-history.md\n"
    "false")
argx_test_expect_contains("${fixtures_STDOUT}" "tests/release/fixtures/CHANGELOG-valid.md"
    "fixtures: the Markdown fixture is not documentation")

expect_docs_only(fixture_with_docs "a Markdown fixture under tests/ with documentation"
    fixture_with_docs "CHANGELOG.md\ntests/release/fixtures/CHANGELOG-history.md\n" "false")

# Markdown in any other directory is not documentation either.
foreach(path IN ITEMS "src/notes.md" "include/argx/README.md" "cmake/README.md"
        "tools/release/README.md" "examples/README.md")
    string(MAKE_C_IDENTIFIER "${path}" name)
    expect_docs_only(other "Markdown at ${path}" "${name}" "${path}\n" "false")
endforeach()

# Paths that look close to documentation but are not.
foreach(path IN ITEMS "CMakeLists.txt" "LICENSE.txt" "src/LICENSE" "docs" "README.markdown"
        "README.MD" ".github/workflows/release.yml" ".github/CODEOWNERS" "docs/../src/argx.c"
        "./README.md" "/README.md" "src/a;docs/b.png")
    string(MAKE_C_IDENTIFIER "x_${path}" name)
    expect_docs_only(other "the path ${path}" "${name}" "${path}\n" "false")
endforeach()

# --- Lists that cannot be judged ---------------------------------------------

expect_docs_only(empty "an empty list" empty "" "false")
expect_docs_only(only_blank "a list of blank lines" only_blank "\n\r\n\n" "false")

# No list given at all, as for a manual run of the CI.
set(output_path "${ARGX_TEST_WORK_DIR}/unset.output")
run_changes(unset "-DARGX_CHANGES_OUTPUT=${output_path}")
argx_test_expect_equal("${unset_RESULT}" "0" "no list: the script succeeds")
argx_test_expect_contains("${unset_STDOUT}" "docs_only=false" "no list: the verdict")
read_output("${output_path}" output)
argx_test_expect_equal("${output}" "docs_only=false\n" "no list: the output file")

# A list file that does not exist, as when "git diff" failed.
set(output_path "${ARGX_TEST_WORK_DIR}/missing.output")
run_changes(missing
    "-DARGX_CHANGED_FILES=${ARGX_TEST_WORK_DIR}/does-not-exist.txt"
    "-DARGX_CHANGES_OUTPUT=${output_path}")
argx_test_expect_equal("${missing_RESULT}" "0" "missing list: the script succeeds")
argx_test_expect_contains("${missing_STDOUT}" "docs_only=false" "missing list: the verdict")
argx_test_expect_contains("${missing_STDOUT}" "does-not-exist.txt"
    "missing list: the path of the list is printed")
read_output("${output_path}" output)
argx_test_expect_equal("${output}" "docs_only=false\n" "missing list: the output file")

# A directory in place of the list.
file(MAKE_DIRECTORY "${ARGX_TEST_WORK_DIR}/a-directory")
set(output_path "${ARGX_TEST_WORK_DIR}/directory.output")
run_changes(directory
    "-DARGX_CHANGED_FILES=${ARGX_TEST_WORK_DIR}/a-directory"
    "-DARGX_CHANGES_OUTPUT=${output_path}")
argx_test_expect_equal("${directory_RESULT}" "0" "directory as list: the script succeeds")
argx_test_expect_contains("${directory_STDOUT}" "docs_only=false" "directory as list: the verdict")
read_output("${output_path}" output)
argx_test_expect_equal("${output}" "docs_only=false\n" "directory as list: the output file")

# --- Without an output file --------------------------------------------------

# The verdict is printed and no file is written, as when run by hand.
file(WRITE "${ARGX_TEST_WORK_DIR}/by-hand.txt" "README.md\n")
file(GLOB before LIST_DIRECTORIES true "${ARGX_TEST_WORK_DIR}/*")
run_changes(by_hand "-DARGX_CHANGED_FILES=by-hand.txt")
file(GLOB after LIST_DIRECTORIES true "${ARGX_TEST_WORK_DIR}/*")
argx_test_expect_equal("${by_hand_RESULT}" "0" "by hand: the script succeeds")
argx_test_expect_contains("${by_hand_STDOUT}" "docs_only=true"
    "by hand: a relative path to the list, the verdict")
argx_test_expect_equal("${after}" "${before}" "by hand: no file is written")

argx_test_finish()
