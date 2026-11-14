# publish.cmake - checks the commit of a merged release pull request against
# the version of the release, and makes the notes of its GitHub Release.
#
#   cmake -D CLINE_RELEASE_VERSION=<version> -D CLINE_RELEASE_REPO_URL=<url>
#         -D CLINE_SOURCE_DIR=<dir> -D CLINE_RELEASE_NOTES=<file>
#         [-D CLINE_RELEASE_OUTPUT=<file>] -P tools/release/publish.cmake
#
#   CLINE_RELEASE_VERSION  The version to publish, such as 0.2.0 or
#                           0.2.0-rc.1, without a leading "v" (the release
#                           workflow takes it from the name of the branch
#                           release/v<version>).
#   CLINE_RELEASE_REPO_URL https://github.com/<owner>/<repo>, for the links to
#                           earlier pre-releases in the notes.
#   CLINE_SOURCE_DIR       The root of the source tree of the merged commit: it
#                           holds include/cline/version.h and CHANGELOG.md.
#   CLINE_RELEASE_NOTES    The file to write the notes of the release to. Its
#                           directory is made when it does not exist.
#   CLINE_RELEASE_OUTPUT   Optional. A file to append the results to, one
#                           "key=value" line each, such as $GITHUB_OUTPUT:
#                             prerelease=<true|false>
#                             title=v<version>
#
# The checks run in this order and the first one that fails stops the script
# with a FATAL_ERROR that starts with "error: " and gives the reason. They all
# run before any file is written, so a failed run writes neither the notes nor
# the output file:
#
#   1. Every required input is given, and the version header and the changelog
#      exist.
#   2. The version is valid for ClineSemver.cmake.
#   3. The version header holds the version, pre-release included, as
#      cline_read_version() reads it.
#   4. The changelog is valid for ClineChangelog.cmake and its newest version
#      is the version.
#   5. The notes are not empty.
#
# The notes are the section of the version in the changelog, without its
# heading, as cline_changelog_notes() gives them: for a release without a
# pre-release, a sentence that refers to the pre-releases of the same
# <major>.<minor>.<patch> right below it follows the section. The changelog is
# read and the notes are written byte for byte (ClineReadBytes.cmake), so the
# notes keep its line endings (LF or CRLF), on Windows too, and they end with
# one.
#
# A version with a pre-release, such as 0.2.0-rc.1, gives prerelease=true; the
# release workflow then marks the GitHub Release as a pre-release and not as
# the latest release. The script reads the two files of the tree and writes
# only the notes and the output file; it touches no network, git or GitHub.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ClineSemver.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ClineChangelog.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ClineReadBytes.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ClineVersion.cmake")

# --- 1. The inputs -------------------------------------------------------------

set(_cline_publish_problems "")
foreach(_cline_publish_input IN ITEMS CLINE_RELEASE_VERSION CLINE_RELEASE_REPO_URL
        CLINE_SOURCE_DIR CLINE_RELEASE_NOTES)
    if(NOT DEFINED ${_cline_publish_input} OR "${${_cline_publish_input}}" STREQUAL "")
        string(APPEND _cline_publish_problems "\n  ${_cline_publish_input} is not set.")
    endif()
endforeach()
if(NOT _cline_publish_problems STREQUAL "")
    message(FATAL_ERROR
        "error: a required input is missing:${_cline_publish_problems}\n"
        "Run the script as\n"
        "  cmake -D CLINE_RELEASE_VERSION=<version> -D CLINE_RELEASE_REPO_URL=<url> "
        "-D CLINE_SOURCE_DIR=<dir> -D CLINE_RELEASE_NOTES=<file> "
        "[-D CLINE_RELEASE_OUTPUT=<file>] -P tools/release/publish.cmake")
endif()

set(_cline_publish_version "${CLINE_RELEASE_VERSION}")
get_filename_component(_cline_publish_root "${CLINE_SOURCE_DIR}" ABSOLUTE)
set(_cline_publish_header "${_cline_publish_root}/include/cline/version.h")
set(_cline_publish_changelog "${_cline_publish_root}/CHANGELOG.md")
foreach(_cline_publish_file IN ITEMS "${_cline_publish_header}" "${_cline_publish_changelog}")
    if(NOT EXISTS "${_cline_publish_file}" OR IS_DIRECTORY "${_cline_publish_file}")
        message(FATAL_ERROR
            "error: ${_cline_publish_file} does not exist. CLINE_SOURCE_DIR must be the root "
            "of a source tree with include/cline/version.h and CHANGELOG.md.")
    endif()
endforeach()

# --- 2. The version --------------------------------------------------------------

cline_semver_parse("${_cline_publish_version}" _cline_publish_v)
if(NOT _cline_publish_v_VALID)
    message(FATAL_ERROR
        "error: the version to publish is not valid: ${_cline_publish_v_ERROR}")
endif()
set(_cline_publish_tag "v${_cline_publish_version}")

# --- 3. The version header -------------------------------------------------------

# Both versions are valid, so they are equal exactly when their text is.
cline_read_version("${_cline_publish_header}" _cline_publish_header_core
    _cline_publish_header_prerelease)
set(_cline_publish_header_version "${_cline_publish_header_core}")
if(NOT _cline_publish_header_prerelease STREQUAL "")
    string(APPEND _cline_publish_header_version "-${_cline_publish_header_prerelease}")
endif()
if(NOT _cline_publish_header_version STREQUAL _cline_publish_version)
    message(FATAL_ERROR
        "error: the version header holds ${_cline_publish_header_version}, not the version "
        "to publish ${_cline_publish_version} (${_cline_publish_header}). The merged commit "
        "does not belong to the release ${_cline_publish_tag}.")
endif()

# --- 4. The changelog -------------------------------------------------------------

cline_read_bytes("${_cline_publish_changelog}" "the changelog" _cline_publish_changelog_text)
cline_changelog_parse("${_cline_publish_changelog_text}" _cline_publish_c)
if(NOT _cline_publish_c_VALID)
    message(FATAL_ERROR
        "error: ${_cline_publish_changelog} is not valid: ${_cline_publish_c_ERROR}")
endif()
if(_cline_publish_c_VERSIONS STREQUAL "")
    message(FATAL_ERROR
        "error: the changelog lists no version; its newest version must be the version to "
        "publish ${_cline_publish_version} (${_cline_publish_changelog}).")
endif()
list(GET _cline_publish_c_VERSIONS 0 _cline_publish_newest)
if(NOT _cline_publish_newest STREQUAL _cline_publish_version)
    message(FATAL_ERROR
        "error: the newest version in the changelog is ${_cline_publish_newest}, not the "
        "version to publish ${_cline_publish_version} (${_cline_publish_changelog}). The "
        "merged commit does not belong to the release ${_cline_publish_tag}.")
endif()

# --- 5. The notes -----------------------------------------------------------------

cline_changelog_notes("${_cline_publish_changelog_text}" "${_cline_publish_version}"
    "${CLINE_RELEASE_REPO_URL}" _cline_publish_notes)
string(STRIP "${_cline_publish_notes}" _cline_publish_notes_stripped)
if(_cline_publish_notes_stripped STREQUAL "")
    message(FATAL_ERROR
        "error: the notes of ${_cline_publish_tag} are empty: the section "
        "[${_cline_publish_version}] of ${_cline_publish_changelog} has no text.")
endif()

# The notes have no line ending at their end; they get one of the kind the
# changelog uses.
string(FIND "${_cline_publish_changelog_text}" "\r\n" _cline_publish_crlf)
if(_cline_publish_crlf EQUAL -1)
    string(APPEND _cline_publish_notes "\n")
else()
    string(APPEND _cline_publish_notes "\r\n")
endif()

if(_cline_publish_v_PRERELEASE STREQUAL "")
    set(_cline_publish_is_prerelease false)
else()
    set(_cline_publish_is_prerelease true)
endif()

# --- Writing -----------------------------------------------------------------------

get_filename_component(_cline_publish_notes_file "${CLINE_RELEASE_NOTES}" ABSOLUTE)
cline_write_bytes("${_cline_publish_notes_file}" "${_cline_publish_notes}" "the notes")

message(STATUS "Release ${_cline_publish_tag} (pre-release: ${_cline_publish_is_prerelease}).")
message(STATUS "The version header and the changelog hold ${_cline_publish_version}.")
message(STATUS "Notes: ${_cline_publish_notes_file}")

if(DEFINED CLINE_RELEASE_OUTPUT AND NOT "${CLINE_RELEASE_OUTPUT}" STREQUAL "")
    file(APPEND "${CLINE_RELEASE_OUTPUT}"
        "prerelease=${_cline_publish_is_prerelease}\n"
        "title=${_cline_publish_tag}\n")
endif()
