# changes.cmake - decides whether a change touches documentation only, so that
# the CI can skip the builds and tests that documentation cannot affect.
#
#   cmake [-D ARGX_CHANGED_FILES=<list-file>] [-D ARGX_CHANGES_OUTPUT=<file>]
#         -P tools/ci/changes.cmake
#
#   ARGX_CHANGED_FILES   A file with one changed path per line, relative to
#                          the root of the repository, as "git diff --name-only"
#                          prints them. Blank lines are ignored, and LF and CRLF
#                          line endings are both accepted. Optional: leave it
#                          out when there is no list.
#   ARGX_CHANGES_OUTPUT  A file to append "docs_only=<true|false>" and a
#                          newline to, such as $GITHUB_OUTPUT. Optional.
#
# A path is documentation when it is one of these:
#
#   - a Markdown file (ending in ".md") directly at the root of the repository,
#     under docs/ or under .github/. Markdown files anywhere else are not
#     documentation: those under tests/, such as the changelog fixtures of the
#     release tests, are inputs of the tests;
#   - any file under docs/;
#   - the file LICENSE at the root of the repository;
#   - any file under .github/ISSUE_TEMPLATE/.
#
# A path that starts with "/" or holds an empty, "." or ".." segment is never
# documentation, so that a path such as "docs/../src/argx.c" is not taken
# for one.
#
# The verdict is docs_only=true only when the list exists, holds at least one
# path and every path in it is documentation. When the list was not given,
# does not exist, cannot be read or is empty, or when a path in it is not
# documentation, the verdict is docs_only=false, so that a change that cannot
# be judged runs every job of the CI.
#
# The verdict and the reason for it (the first path that is not documentation,
# or why there is no list to judge) are printed on the standard output. The
# script ends with exit code 0 whatever the verdict; it fails only when the
# output file cannot be written. It reads nothing but the list and touches no
# network, git or GitHub, so it runs the same way by hand.

cmake_minimum_required(VERSION 3.21)

# Sets <out-var> to TRUE when <path> is documentation by the rules above, and
# to FALSE otherwise.
function(_argx_changes_is_docs path out_var)
    set(${out_var} FALSE PARENT_SCOPE)

    # A path made of plain segments only.
    if(path MATCHES "^/" OR path MATCHES "/$" OR path MATCHES "//"
            OR "/${path}/" MATCHES "/\\.\\.?/")
        return()
    endif()

    if(path MATCHES "^[^/]+\\.md$"
            OR path MATCHES "^docs/."
            OR path MATCHES "^\\.github/.+\\.md$"
            OR path STREQUAL "LICENSE"
            OR path MATCHES "^\\.github/ISSUE_TEMPLATE/.")
        set(${out_var} TRUE PARENT_SCOPE)
    endif()
endfunction()

# Prints the verdict and the reason for it, appends the verdict to the output
# file when one is given.
function(_argx_changes_finish docs_only reason)
    message(STATUS "${reason}")
    message(STATUS "docs_only=${docs_only}")
    if(DEFINED ARGX_CHANGES_OUTPUT AND NOT "${ARGX_CHANGES_OUTPUT}" STREQUAL "")
        file(APPEND "${ARGX_CHANGES_OUTPUT}" "docs_only=${docs_only}\n")
    endif()
endfunction()

if(NOT DEFINED ARGX_CHANGED_FILES OR "${ARGX_CHANGED_FILES}" STREQUAL "")
    _argx_changes_finish(false
        "No list of changed files was given; every job runs.")
    return()
endif()

get_filename_component(argx_changes_list "${ARGX_CHANGED_FILES}" ABSOLUTE)
if(NOT EXISTS "${argx_changes_list}" OR IS_DIRECTORY "${argx_changes_list}")
    _argx_changes_finish(false
        "The list of changed files ${argx_changes_list} does not exist; every job runs.")
    return()
endif()

file(READ "${argx_changes_list}" argx_changes_text)

# The paths are taken one line at a time with string(FIND) rather than turned
# into a CMake list, so that a path holding ";" or "[" stays one path.
set(argx_changes_count 0)
set(argx_changes_first_other "")
string(APPEND argx_changes_text "\n")
while(NOT argx_changes_text STREQUAL "")
    string(FIND "${argx_changes_text}" "\n" argx_changes_end)
    string(SUBSTRING "${argx_changes_text}" 0 ${argx_changes_end} argx_changes_path)
    math(EXPR argx_changes_end "${argx_changes_end} + 1")
    string(SUBSTRING "${argx_changes_text}" ${argx_changes_end} -1 argx_changes_text)

    string(REGEX REPLACE "\r$" "" argx_changes_path "${argx_changes_path}")
    if(argx_changes_path STREQUAL "")
        continue()
    endif()
    math(EXPR argx_changes_count "${argx_changes_count} + 1")
    _argx_changes_is_docs("${argx_changes_path}" argx_changes_is_docs)
    if(NOT argx_changes_is_docs)
        set(argx_changes_first_other "${argx_changes_path}")
        break()
    endif()
endwhile()

if(NOT argx_changes_first_other STREQUAL "")
    _argx_changes_finish(false
        "Not documentation: ${argx_changes_first_other}; every job runs.")
elseif(argx_changes_count EQUAL 0)
    _argx_changes_finish(false
        "The list of changed files ${argx_changes_list} is empty; every job runs.")
else()
    _argx_changes_finish(true
        "Every changed path is documentation (${argx_changes_count} in all); the builds and tests are skipped.")
endif()
