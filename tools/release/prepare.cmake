# prepare.cmake - checks a release before its pull request is opened, and
# rewrites the version header and the changelog for it.
#
#   cmake -D ARGX_RELEASE_VERSION=<version> -D ARGX_RELEASE_DATE=<YYYY-MM-DD>
#         -D ARGX_RELEASE_REPO_URL=<url> -D ARGX_SOURCE_DIR=<dir>
#         -D ARGX_RELEASE_TAGS_FILE=<file> [-D ARGX_RELEASE_OUTPUT=<file>]
#         [-D ARGX_RELEASE_CHECK_ONLY=ON] -P tools/release/prepare.cmake
#
#   ARGX_RELEASE_VERSION     The version to release, such as 0.2.0 or
#                            0.2.0-rc.1, without a leading "v".
#   ARGX_RELEASE_DATE        The date of the release, YYYY-MM-DD (the release
#                            workflow passes the day in UTC).
#   ARGX_RELEASE_REPO_URL    https://github.com/<owner>/<repo>, for the links
#                            at the end of the changelog.
#   ARGX_SOURCE_DIR          The root of the source tree to rewrite: it holds
#                            include/argx/version.h and CHANGELOG.md. It
#                            may be a copy of the working tree, to try a
#                            release by hand.
#   ARGX_RELEASE_TAGS_FILE   A file with the existing tags of the repository,
#                            one per line (it may be empty). Only a tag that
#                            is "v" followed by a valid version counts as a
#                            release; other lines are ignored.
#   ARGX_RELEASE_OUTPUT      Optional. A file to append the results to, one
#                            "key=value" line each, such as $GITHUB_OUTPUT:
#                                version=<version>
#                                tag=v<version>
#                                prerelease=<true|false>
#   ARGX_RELEASE_CHECK_ONLY  Optional. When true, runs every check and
#                            prepares the new files, but writes nothing: not
#                            the two files and not the output file.
#
# The checks run in this order and the first one that fails stops the script
# with a FATAL_ERROR that starts with "error: " and gives the reason. They all
# run before any file is written, so a failed run leaves every file as it was:
#
#   1. Every required input is given, the date is of the form YYYY-MM-DD, and
#      the tags file, the version header and the changelog exist.
#   2. The version is valid for ArgXSemver.cmake: <major>.<minor>.<patch>
#      with an optional pre-release, no build metadata, and each number in
#      0-255.
#   3. The tag v<version> does not exist.
#   4. The version is higher, by SemVer 2.0.0 precedence, than every release
#      tag.
#   5. The version is not lower than the version in the version header. The
#      version of the header itself is accepted while it has no tag, as for the
#      first release; once it has one, check 3 or 4 has stopped the script.
#   6. The changelog is valid for ArgXChangelog.cmake, does not list the
#      version yet and has at least one entry under [Unreleased].
#
# Then the new text of both files is made in full (which may still stop the
# script, for instance when the changelog lists a higher version), and only
# then are the version header and the changelog written. No other file of the
# tree is touched. The header is read back with argx_read_version() to make
# sure it holds the new version. Both files are read and written byte for byte
# (ArgXReadBytes.cmake), so their line endings (LF or CRLF) are kept, on
# Windows too.
#
# The script prints what it changed. Run it twice with the same input and the
# second run stops at check 6, since the changelog lists the version then. It
# reads and writes only the files above and touches no network, git or GitHub.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ArgXSemver.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ArgXChangelog.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ArgXVersionHeader.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ArgXReadBytes.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ArgXVersion.cmake")

# --- 1. The inputs -------------------------------------------------------------

set(_argx_prepare_problems "")
foreach(_argx_prepare_input IN ITEMS ARGX_RELEASE_VERSION ARGX_RELEASE_DATE
        ARGX_RELEASE_REPO_URL ARGX_SOURCE_DIR ARGX_RELEASE_TAGS_FILE)
    if(NOT DEFINED ${_argx_prepare_input} OR "${${_argx_prepare_input}}" STREQUAL "")
        string(APPEND _argx_prepare_problems "\n  ${_argx_prepare_input} is not set.")
    endif()
endforeach()
if(NOT _argx_prepare_problems STREQUAL "")
    message(FATAL_ERROR
        "error: a required input is missing:${_argx_prepare_problems}\n"
        "Run the script as\n"
        "  cmake -D ARGX_RELEASE_VERSION=<version> -D ARGX_RELEASE_DATE=<YYYY-MM-DD> "
        "-D ARGX_RELEASE_REPO_URL=<url> -D ARGX_SOURCE_DIR=<dir> "
        "-D ARGX_RELEASE_TAGS_FILE=<file> [-D ARGX_RELEASE_OUTPUT=<file>] "
        "[-D ARGX_RELEASE_CHECK_ONLY=ON] -P tools/release/prepare.cmake")
endif()

set(_argx_prepare_version "${ARGX_RELEASE_VERSION}")
set(_argx_prepare_date "${ARGX_RELEASE_DATE}")
if(NOT _argx_prepare_date MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$")
    message(FATAL_ERROR
        "error: the date \"${_argx_prepare_date}\" (ARGX_RELEASE_DATE) "
        "is not of the form YYYY-MM-DD.")
endif()

get_filename_component(_argx_prepare_tags_file "${ARGX_RELEASE_TAGS_FILE}" ABSOLUTE)
if(NOT EXISTS "${_argx_prepare_tags_file}" OR IS_DIRECTORY "${_argx_prepare_tags_file}")
    message(FATAL_ERROR
        "error: the list of tags ${_argx_prepare_tags_file} (ARGX_RELEASE_TAGS_FILE) "
        "does not exist. Pass a file with one tag per line; it may be empty.")
endif()

get_filename_component(_argx_prepare_root "${ARGX_SOURCE_DIR}" ABSOLUTE)
set(_argx_prepare_header "${_argx_prepare_root}/include/argx/version.h")
set(_argx_prepare_changelog "${_argx_prepare_root}/CHANGELOG.md")
foreach(_argx_prepare_file IN ITEMS "${_argx_prepare_header}" "${_argx_prepare_changelog}")
    if(NOT EXISTS "${_argx_prepare_file}" OR IS_DIRECTORY "${_argx_prepare_file}")
        message(FATAL_ERROR
            "error: ${_argx_prepare_file} does not exist. ARGX_SOURCE_DIR must be the root "
            "of a source tree with include/argx/version.h and CHANGELOG.md.")
    endif()
endforeach()

# --- 2. The version --------------------------------------------------------------

argx_semver_parse("${_argx_prepare_version}" _argx_prepare_v)
if(NOT _argx_prepare_v_VALID)
    message(FATAL_ERROR
        "error: the version to release is not valid: ${_argx_prepare_v_ERROR}")
endif()
set(_argx_prepare_tag "v${_argx_prepare_version}")

# --- 3. and 4. The existing tags -------------------------------------------------

# The lines are taken one at a time with string(FIND) rather than turned into
# a CMake list, so that a line holding ";" stays one line. Line endings and
# blanks around a tag are dropped.
file(READ "${_argx_prepare_tags_file}" _argx_prepare_rest)
string(APPEND _argx_prepare_rest "\n")
set(_argx_prepare_tag_exists FALSE)
set(_argx_prepare_highest "")
while(NOT _argx_prepare_rest STREQUAL "")
    string(FIND "${_argx_prepare_rest}" "\n" _argx_prepare_end)
    string(SUBSTRING "${_argx_prepare_rest}" 0 ${_argx_prepare_end} _argx_prepare_line)
    math(EXPR _argx_prepare_end "${_argx_prepare_end} + 1")
    string(SUBSTRING "${_argx_prepare_rest}" ${_argx_prepare_end} -1 _argx_prepare_rest)
    string(STRIP "${_argx_prepare_line}" _argx_prepare_line)

    if(_argx_prepare_line STREQUAL _argx_prepare_tag)
        set(_argx_prepare_tag_exists TRUE)
    endif()
    # A release tag is "v" and a valid version.
    if(NOT _argx_prepare_line MATCHES "^v(.+)$")
        continue()
    endif()
    set(_argx_prepare_tag_version "${CMAKE_MATCH_1}")
    argx_semver_parse("${_argx_prepare_tag_version}" _argx_prepare_t)
    if(NOT _argx_prepare_t_VALID)
        continue()
    endif()
    if(_argx_prepare_highest STREQUAL "")
        set(_argx_prepare_highest "${_argx_prepare_tag_version}")
    else()
        argx_semver_compare("${_argx_prepare_tag_version}" "${_argx_prepare_highest}"
            _argx_prepare_order)
        if(_argx_prepare_order EQUAL 1)
            set(_argx_prepare_highest "${_argx_prepare_tag_version}")
        endif()
    endif()
endwhile()

if(_argx_prepare_tag_exists)
    message(FATAL_ERROR
        "error: the tag ${_argx_prepare_tag} already exists; "
        "the version ${_argx_prepare_version} has been released. Choose a higher version.")
endif()

if(NOT _argx_prepare_highest STREQUAL "")
    argx_semver_compare("${_argx_prepare_version}" "${_argx_prepare_highest}"
        _argx_prepare_order)
    if(NOT _argx_prepare_order EQUAL 1)
        message(FATAL_ERROR
            "error: the version ${_argx_prepare_version} is not higher than the release tag "
            "v${_argx_prepare_highest}. A release must be higher, by SemVer precedence, "
            "than every release before it.")
    endif()
endif()

# --- 5. The version header -------------------------------------------------------

argx_read_version("${_argx_prepare_header}" _argx_prepare_header_core
    _argx_prepare_header_prerelease)
set(_argx_prepare_header_version "${_argx_prepare_header_core}")
if(NOT _argx_prepare_header_prerelease STREQUAL "")
    string(APPEND _argx_prepare_header_version "-${_argx_prepare_header_prerelease}")
endif()
argx_semver_compare("${_argx_prepare_version}" "${_argx_prepare_header_version}"
    _argx_prepare_order)
if(_argx_prepare_order EQUAL -1)
    message(FATAL_ERROR
        "error: the version ${_argx_prepare_version} is lower than the version "
        "${_argx_prepare_header_version} of the version header ${_argx_prepare_header}.")
endif()
# The version of the header is accepted only while it has no tag. Check 3
# has made sure of that already; this keeps the rule visible here.
if(_argx_prepare_order EQUAL 0 AND _argx_prepare_tag_exists)
    message(FATAL_ERROR
        "error: the version ${_argx_prepare_version} of the version header "
        "has been released as ${_argx_prepare_tag}.")
endif()

# --- 6. The changelog -------------------------------------------------------------

argx_read_bytes("${_argx_prepare_changelog}" "the changelog"
    _argx_prepare_changelog_text)
argx_changelog_parse("${_argx_prepare_changelog_text}" _argx_prepare_c)
if(NOT _argx_prepare_c_VALID)
    message(FATAL_ERROR
        "error: ${_argx_prepare_changelog} is not valid: ${_argx_prepare_c_ERROR}")
endif()
if(NOT _argx_prepare_c_VERSIONS STREQUAL "" AND _argx_prepare_version IN_LIST _argx_prepare_c_VERSIONS)
    message(FATAL_ERROR
        "error: the version ${_argx_prepare_version} is already listed in "
        "${_argx_prepare_changelog}.")
endif()
if(_argx_prepare_c_UNRELEASED_ENTRIES EQUAL 0)
    message(FATAL_ERROR
        "error: [Unreleased] has no entry in ${_argx_prepare_changelog}. "
        "Record the changes of the release under it first.")
endif()

# --- The new files -----------------------------------------------------------------

# Both texts are made in full before either file is written; each function
# stops with a FATAL_ERROR when it cannot make its text.
argx_read_bytes("${_argx_prepare_header}" "the version header"
    _argx_prepare_header_text)
argx_version_header_render("${_argx_prepare_header_text}"
    "${_argx_prepare_v_MAJOR}" "${_argx_prepare_v_MINOR}" "${_argx_prepare_v_PATCH}"
    "${_argx_prepare_v_PRERELEASE}" _argx_prepare_new_header)
argx_changelog_promote("${_argx_prepare_changelog_text}" "${_argx_prepare_version}"
    "${_argx_prepare_date}" "${ARGX_RELEASE_REPO_URL}" _argx_prepare_new_changelog)

if(_argx_prepare_v_PRERELEASE STREQUAL "")
    set(_argx_prepare_is_prerelease false)
else()
    set(_argx_prepare_is_prerelease true)
endif()

message(STATUS "Release ${_argx_prepare_tag} (pre-release: ${_argx_prepare_is_prerelease}).")
if(_argx_prepare_new_header STREQUAL _argx_prepare_header_text)
    set(_argx_prepare_header_change
        "${_argx_prepare_header}: already holds ${_argx_prepare_version}")
else()
    set(_argx_prepare_header_change
        "${_argx_prepare_header}: ${_argx_prepare_header_version} -> ${_argx_prepare_version}")
endif()
set(_argx_prepare_changelog_change
    "${_argx_prepare_changelog}: [Unreleased] -> [${_argx_prepare_version}] - ${_argx_prepare_date}")

# Both new texts must be writable byte for byte on this host (on Windows, they
# must end with a line ending), so that either both files are written or
# neither is.
argx_check_write_bytes("${_argx_prepare_header}" "${_argx_prepare_new_header}"
    "the version header")
argx_check_write_bytes("${_argx_prepare_changelog}" "${_argx_prepare_new_changelog}"
    "the changelog")

if(ARGX_RELEASE_CHECK_ONLY)
    message(STATUS "Every check passed. Check only: nothing was written. A release would change")
    message(STATUS "  ${_argx_prepare_header_change}")
    message(STATUS "  ${_argx_prepare_changelog_change}")
    return()
endif()

# --- Writing -----------------------------------------------------------------------

argx_write_bytes("${_argx_prepare_header}" "${_argx_prepare_new_header}" "the version header")
argx_write_bytes("${_argx_prepare_changelog}" "${_argx_prepare_new_changelog}" "the changelog")

# Read the header back as the build reads it.
argx_read_version("${_argx_prepare_header}" _argx_prepare_read_core
    _argx_prepare_read_prerelease)
if(NOT _argx_prepare_read_core STREQUAL _argx_prepare_v_CORE
        OR NOT _argx_prepare_read_prerelease STREQUAL _argx_prepare_v_PRERELEASE)
    message(FATAL_ERROR
        "error: the version header ${_argx_prepare_header} was rewritten, but it reads as "
        "\"${_argx_prepare_read_core}\" with the pre-release \"${_argx_prepare_read_prerelease}\" "
        "instead of \"${_argx_prepare_v_CORE}\" with the pre-release "
        "\"${_argx_prepare_v_PRERELEASE}\".")
endif()

message(STATUS "Changed:")
message(STATUS "  ${_argx_prepare_header_change}")
message(STATUS "  ${_argx_prepare_changelog_change}")

if(DEFINED ARGX_RELEASE_OUTPUT AND NOT "${ARGX_RELEASE_OUTPUT}" STREQUAL "")
    file(APPEND "${ARGX_RELEASE_OUTPUT}"
        "version=${_argx_prepare_version}\n"
        "tag=${_argx_prepare_tag}\n"
        "prerelease=${_argx_prepare_is_prerelease}\n")
endif()
