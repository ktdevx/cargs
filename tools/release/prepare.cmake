# prepare.cmake - checks a release before its pull request is opened, and
# rewrites the version header and the changelog for it.
#
#   cmake -D CLINE_RELEASE_VERSION=<version> -D CLINE_RELEASE_DATE=<YYYY-MM-DD>
#         -D CLINE_RELEASE_REPO_URL=<url> -D CLINE_SOURCE_DIR=<dir>
#         -D CLINE_RELEASE_TAGS_FILE=<file> [-D CLINE_RELEASE_OUTPUT=<file>]
#         [-D CLINE_RELEASE_CHECK_ONLY=ON] -P tools/release/prepare.cmake
#
#   CLINE_RELEASE_VERSION     The version to release, such as 0.2.0 or
#                              0.2.0-rc.1, without a leading "v".
#   CLINE_RELEASE_DATE        The date of the release, YYYY-MM-DD (the release
#                              workflow passes the day in UTC).
#   CLINE_RELEASE_REPO_URL    https://github.com/<owner>/<repo>, for the links
#                              at the end of the changelog.
#   CLINE_SOURCE_DIR          The root of the source tree to rewrite: it holds
#                              include/cline/version.h and CHANGELOG.md. It
#                              may be a copy of the working tree, to try a
#                              release by hand.
#   CLINE_RELEASE_TAGS_FILE   A file with the existing tags of the repository,
#                              one per line (it may be empty). Only a tag that
#                              is "v" followed by a valid version counts as a
#                              release; other lines are ignored.
#   CLINE_RELEASE_OUTPUT      Optional. A file to append the results to, one
#                              "key=value" line each, such as $GITHUB_OUTPUT:
#                                version=<version>
#                                tag=v<version>
#                                prerelease=<true|false>
#   CLINE_RELEASE_CHECK_ONLY  Optional. When true, runs every check and
#                              prepares the new files, but writes nothing: not
#                              the two files and not the output file.
#
# The checks run in this order and the first one that fails stops the script
# with a FATAL_ERROR that starts with "error: " and gives the reason. They all
# run before any file is written, so a failed run leaves every file as it was:
#
#   1. Every required input is given, the date is of the form YYYY-MM-DD, and
#      the tags file, the version header and the changelog exist.
#   2. The version is valid for ClineSemver.cmake: <major>.<minor>.<patch>
#      with an optional pre-release, no build metadata, and each number in
#      0-255.
#   3. The tag v<version> does not exist.
#   4. The version is higher, by SemVer 2.0.0 precedence, than every release
#      tag.
#   5. The version is not lower than the version in the version header. The
#      version of the header itself is accepted while it has no tag, as for the
#      first release; once it has one, check 3 or 4 has stopped the script.
#   6. The changelog is valid for ClineChangelog.cmake, does not list the
#      version yet and has at least one entry under [Unreleased].
#
# Then the new text of both files is made in full (which may still stop the
# script, for instance when the changelog lists a higher version), and only
# then are the version header and the changelog written. No other file of the
# tree is touched. The header is read back with cline_read_version() to make
# sure it holds the new version. Both files are read and written byte for byte
# (ClineReadBytes.cmake), so their line endings (LF or CRLF) are kept, on
# Windows too.
#
# The script prints what it changed. Run it twice with the same input and the
# second run stops at check 6, since the changelog lists the version then. It
# reads and writes only the files above and touches no network, git or GitHub.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ClineSemver.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ClineChangelog.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ClineVersionHeader.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ClineReadBytes.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ClineVersion.cmake")

# --- 1. The inputs -------------------------------------------------------------

set(_cline_prepare_problems "")
foreach(_cline_prepare_input IN ITEMS CLINE_RELEASE_VERSION CLINE_RELEASE_DATE
        CLINE_RELEASE_REPO_URL CLINE_SOURCE_DIR CLINE_RELEASE_TAGS_FILE)
    if(NOT DEFINED ${_cline_prepare_input} OR "${${_cline_prepare_input}}" STREQUAL "")
        string(APPEND _cline_prepare_problems "\n  ${_cline_prepare_input} is not set.")
    endif()
endforeach()
if(NOT _cline_prepare_problems STREQUAL "")
    message(FATAL_ERROR
        "error: a required input is missing:${_cline_prepare_problems}\n"
        "Run the script as\n"
        "  cmake -D CLINE_RELEASE_VERSION=<version> -D CLINE_RELEASE_DATE=<YYYY-MM-DD> "
        "-D CLINE_RELEASE_REPO_URL=<url> -D CLINE_SOURCE_DIR=<dir> "
        "-D CLINE_RELEASE_TAGS_FILE=<file> [-D CLINE_RELEASE_OUTPUT=<file>] "
        "[-D CLINE_RELEASE_CHECK_ONLY=ON] -P tools/release/prepare.cmake")
endif()

set(_cline_prepare_version "${CLINE_RELEASE_VERSION}")
set(_cline_prepare_date "${CLINE_RELEASE_DATE}")
if(NOT _cline_prepare_date MATCHES "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]$")
    message(FATAL_ERROR
        "error: the date \"${_cline_prepare_date}\" (CLINE_RELEASE_DATE) "
        "is not of the form YYYY-MM-DD.")
endif()

get_filename_component(_cline_prepare_tags_file "${CLINE_RELEASE_TAGS_FILE}" ABSOLUTE)
if(NOT EXISTS "${_cline_prepare_tags_file}" OR IS_DIRECTORY "${_cline_prepare_tags_file}")
    message(FATAL_ERROR
        "error: the list of tags ${_cline_prepare_tags_file} (CLINE_RELEASE_TAGS_FILE) "
        "does not exist. Pass a file with one tag per line; it may be empty.")
endif()

get_filename_component(_cline_prepare_root "${CLINE_SOURCE_DIR}" ABSOLUTE)
set(_cline_prepare_header "${_cline_prepare_root}/include/cline/version.h")
set(_cline_prepare_changelog "${_cline_prepare_root}/CHANGELOG.md")
foreach(_cline_prepare_file IN ITEMS "${_cline_prepare_header}" "${_cline_prepare_changelog}")
    if(NOT EXISTS "${_cline_prepare_file}" OR IS_DIRECTORY "${_cline_prepare_file}")
        message(FATAL_ERROR
            "error: ${_cline_prepare_file} does not exist. CLINE_SOURCE_DIR must be the root "
            "of a source tree with include/cline/version.h and CHANGELOG.md.")
    endif()
endforeach()

# --- 2. The version --------------------------------------------------------------

cline_semver_parse("${_cline_prepare_version}" _cline_prepare_v)
if(NOT _cline_prepare_v_VALID)
    message(FATAL_ERROR
        "error: the version to release is not valid: ${_cline_prepare_v_ERROR}")
endif()
set(_cline_prepare_tag "v${_cline_prepare_version}")

# --- 3. and 4. The existing tags -------------------------------------------------

# The lines are taken one at a time with string(FIND) rather than turned into
# a CMake list, so that a line holding ";" stays one line. Line endings and
# blanks around a tag are dropped.
file(READ "${_cline_prepare_tags_file}" _cline_prepare_rest)
string(APPEND _cline_prepare_rest "\n")
set(_cline_prepare_tag_exists FALSE)
set(_cline_prepare_highest "")
while(NOT _cline_prepare_rest STREQUAL "")
    string(FIND "${_cline_prepare_rest}" "\n" _cline_prepare_end)
    string(SUBSTRING "${_cline_prepare_rest}" 0 ${_cline_prepare_end} _cline_prepare_line)
    math(EXPR _cline_prepare_end "${_cline_prepare_end} + 1")
    string(SUBSTRING "${_cline_prepare_rest}" ${_cline_prepare_end} -1 _cline_prepare_rest)
    string(STRIP "${_cline_prepare_line}" _cline_prepare_line)

    if(_cline_prepare_line STREQUAL _cline_prepare_tag)
        set(_cline_prepare_tag_exists TRUE)
    endif()
    # A release tag is "v" and a valid version.
    if(NOT _cline_prepare_line MATCHES "^v(.+)$")
        continue()
    endif()
    set(_cline_prepare_tag_version "${CMAKE_MATCH_1}")
    cline_semver_parse("${_cline_prepare_tag_version}" _cline_prepare_t)
    if(NOT _cline_prepare_t_VALID)
        continue()
    endif()
    if(_cline_prepare_highest STREQUAL "")
        set(_cline_prepare_highest "${_cline_prepare_tag_version}")
    else()
        cline_semver_compare("${_cline_prepare_tag_version}" "${_cline_prepare_highest}"
            _cline_prepare_order)
        if(_cline_prepare_order EQUAL 1)
            set(_cline_prepare_highest "${_cline_prepare_tag_version}")
        endif()
    endif()
endwhile()

if(_cline_prepare_tag_exists)
    message(FATAL_ERROR
        "error: the tag ${_cline_prepare_tag} already exists; "
        "the version ${_cline_prepare_version} has been released. Choose a higher version.")
endif()

if(NOT _cline_prepare_highest STREQUAL "")
    cline_semver_compare("${_cline_prepare_version}" "${_cline_prepare_highest}"
        _cline_prepare_order)
    if(NOT _cline_prepare_order EQUAL 1)
        message(FATAL_ERROR
            "error: the version ${_cline_prepare_version} is not higher than the release tag "
            "v${_cline_prepare_highest}. A release must be higher, by SemVer precedence, "
            "than every release before it.")
    endif()
endif()

# --- 5. The version header -------------------------------------------------------

cline_read_version("${_cline_prepare_header}" _cline_prepare_header_core
    _cline_prepare_header_prerelease)
set(_cline_prepare_header_version "${_cline_prepare_header_core}")
if(NOT _cline_prepare_header_prerelease STREQUAL "")
    string(APPEND _cline_prepare_header_version "-${_cline_prepare_header_prerelease}")
endif()
cline_semver_compare("${_cline_prepare_version}" "${_cline_prepare_header_version}"
    _cline_prepare_order)
if(_cline_prepare_order EQUAL -1)
    message(FATAL_ERROR
        "error: the version ${_cline_prepare_version} is lower than the version "
        "${_cline_prepare_header_version} of the version header ${_cline_prepare_header}.")
endif()
# The version of the header is accepted only while it has no tag. Check 3
# has made sure of that already; this keeps the rule visible here.
if(_cline_prepare_order EQUAL 0 AND _cline_prepare_tag_exists)
    message(FATAL_ERROR
        "error: the version ${_cline_prepare_version} of the version header "
        "has been released as ${_cline_prepare_tag}.")
endif()

# --- 6. The changelog -------------------------------------------------------------

cline_read_bytes("${_cline_prepare_changelog}" "the changelog"
    _cline_prepare_changelog_text)
cline_changelog_parse("${_cline_prepare_changelog_text}" _cline_prepare_c)
if(NOT _cline_prepare_c_VALID)
    message(FATAL_ERROR
        "error: ${_cline_prepare_changelog} is not valid: ${_cline_prepare_c_ERROR}")
endif()
if(NOT _cline_prepare_c_VERSIONS STREQUAL "" AND _cline_prepare_version IN_LIST _cline_prepare_c_VERSIONS)
    message(FATAL_ERROR
        "error: the version ${_cline_prepare_version} is already listed in "
        "${_cline_prepare_changelog}.")
endif()
if(_cline_prepare_c_UNRELEASED_ENTRIES EQUAL 0)
    message(FATAL_ERROR
        "error: [Unreleased] has no entry in ${_cline_prepare_changelog}. "
        "Record the changes of the release under it first.")
endif()

# --- The new files -----------------------------------------------------------------

# Both texts are made in full before either file is written; each function
# stops with a FATAL_ERROR when it cannot make its text.
cline_read_bytes("${_cline_prepare_header}" "the version header"
    _cline_prepare_header_text)
cline_version_header_render("${_cline_prepare_header_text}"
    "${_cline_prepare_v_MAJOR}" "${_cline_prepare_v_MINOR}" "${_cline_prepare_v_PATCH}"
    "${_cline_prepare_v_PRERELEASE}" _cline_prepare_new_header)
cline_changelog_promote("${_cline_prepare_changelog_text}" "${_cline_prepare_version}"
    "${_cline_prepare_date}" "${CLINE_RELEASE_REPO_URL}" _cline_prepare_new_changelog)

if(_cline_prepare_v_PRERELEASE STREQUAL "")
    set(_cline_prepare_is_prerelease false)
else()
    set(_cline_prepare_is_prerelease true)
endif()

message(STATUS "Release ${_cline_prepare_tag} (pre-release: ${_cline_prepare_is_prerelease}).")
if(_cline_prepare_new_header STREQUAL _cline_prepare_header_text)
    set(_cline_prepare_header_change
        "${_cline_prepare_header}: already holds ${_cline_prepare_version}")
else()
    set(_cline_prepare_header_change
        "${_cline_prepare_header}: ${_cline_prepare_header_version} -> ${_cline_prepare_version}")
endif()
set(_cline_prepare_changelog_change
    "${_cline_prepare_changelog}: [Unreleased] -> [${_cline_prepare_version}] - ${_cline_prepare_date}")

# Both new texts must be writable byte for byte on this host (on Windows, they
# must end with a line ending), so that either both files are written or
# neither is.
cline_check_write_bytes("${_cline_prepare_header}" "${_cline_prepare_new_header}"
    "the version header")
cline_check_write_bytes("${_cline_prepare_changelog}" "${_cline_prepare_new_changelog}"
    "the changelog")

if(CLINE_RELEASE_CHECK_ONLY)
    message(STATUS "Every check passed. Check only: nothing was written. A release would change")
    message(STATUS "  ${_cline_prepare_header_change}")
    message(STATUS "  ${_cline_prepare_changelog_change}")
    return()
endif()

# --- Writing -----------------------------------------------------------------------

cline_write_bytes("${_cline_prepare_header}" "${_cline_prepare_new_header}" "the version header")
cline_write_bytes("${_cline_prepare_changelog}" "${_cline_prepare_new_changelog}" "the changelog")

# Read the header back as the build reads it.
cline_read_version("${_cline_prepare_header}" _cline_prepare_read_core
    _cline_prepare_read_prerelease)
if(NOT _cline_prepare_read_core STREQUAL _cline_prepare_v_CORE
        OR NOT _cline_prepare_read_prerelease STREQUAL _cline_prepare_v_PRERELEASE)
    message(FATAL_ERROR
        "error: the version header ${_cline_prepare_header} was rewritten, but it reads as "
        "\"${_cline_prepare_read_core}\" with the pre-release \"${_cline_prepare_read_prerelease}\" "
        "instead of \"${_cline_prepare_v_CORE}\" with the pre-release "
        "\"${_cline_prepare_v_PRERELEASE}\".")
endif()

message(STATUS "Changed:")
message(STATUS "  ${_cline_prepare_header_change}")
message(STATUS "  ${_cline_prepare_changelog_change}")

if(DEFINED CLINE_RELEASE_OUTPUT AND NOT "${CLINE_RELEASE_OUTPUT}" STREQUAL "")
    file(APPEND "${CLINE_RELEASE_OUTPUT}"
        "version=${_cline_prepare_version}\n"
        "tag=${_cline_prepare_tag}\n"
        "prerelease=${_cline_prepare_is_prerelease}\n")
endif()
