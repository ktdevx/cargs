# publish.cmake - checks the commit of a merged release pull request against
# the version of the release, and makes the notes of its GitHub Release.
#
#   cmake -D CLARGS_RELEASE_VERSION=<version> -D CLARGS_RELEASE_REPO_URL=<url>
#         -D CLARGS_SOURCE_DIR=<dir> -D CLARGS_RELEASE_NOTES=<file>
#         [-D CLARGS_RELEASE_OUTPUT=<file>] -P tools/release/publish.cmake
#
#   CLARGS_RELEASE_VERSION  The version to publish, such as 0.2.0 or
#                           0.2.0-rc.1, without a leading "v" (the release
#                           workflow takes it from the name of the branch
#                           release/v<version>).
#   CLARGS_RELEASE_REPO_URL https://github.com/<owner>/<repo>, for the links to
#                           earlier pre-releases in the notes.
#   CLARGS_SOURCE_DIR       The root of the source tree of the merged commit: it
#                           holds include/clargs/version.h and CHANGELOG.md.
#   CLARGS_RELEASE_NOTES    The file to write the notes of the release to. Its
#                           directory is made when it does not exist.
#   CLARGS_RELEASE_OUTPUT   Optional. A file to append the results to, one
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
#   2. The version is valid for ClargsSemver.cmake.
#   3. The version header holds the version, pre-release included, as
#      clargs_read_version() reads it.
#   4. The changelog is valid for ClargsChangelog.cmake and its newest version
#      is the version.
#   5. The notes are not empty.
#
# The notes are the section of the version in the changelog, without its
# heading, as clargs_changelog_notes() gives them: for a release without a
# pre-release, a sentence that refers to the pre-releases of the same
# <major>.<minor>.<patch> right below it follows the section. The changelog is
# read and the notes are written byte for byte (ClargsReadBytes.cmake), so the
# notes keep its line endings (LF or CRLF), on Windows too, and they end with
# one.
#
# A version with a pre-release, such as 0.2.0-rc.1, gives prerelease=true; the
# release workflow then marks the GitHub Release as a pre-release and not as
# the latest release. The script reads the two files of the tree and writes
# only the notes and the output file; it touches no network, git or GitHub.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ClargsSemver.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ClargsChangelog.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ClargsReadBytes.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ClargsVersion.cmake")

# --- 1. The inputs -------------------------------------------------------------

set(_clargs_publish_problems "")
foreach(_clargs_publish_input IN ITEMS CLARGS_RELEASE_VERSION CLARGS_RELEASE_REPO_URL
        CLARGS_SOURCE_DIR CLARGS_RELEASE_NOTES)
    if(NOT DEFINED ${_clargs_publish_input} OR "${${_clargs_publish_input}}" STREQUAL "")
        string(APPEND _clargs_publish_problems "\n  ${_clargs_publish_input} is not set.")
    endif()
endforeach()
if(NOT _clargs_publish_problems STREQUAL "")
    message(FATAL_ERROR
        "error: a required input is missing:${_clargs_publish_problems}\n"
        "Run the script as\n"
        "  cmake -D CLARGS_RELEASE_VERSION=<version> -D CLARGS_RELEASE_REPO_URL=<url> "
        "-D CLARGS_SOURCE_DIR=<dir> -D CLARGS_RELEASE_NOTES=<file> "
        "[-D CLARGS_RELEASE_OUTPUT=<file>] -P tools/release/publish.cmake")
endif()

set(_clargs_publish_version "${CLARGS_RELEASE_VERSION}")
get_filename_component(_clargs_publish_root "${CLARGS_SOURCE_DIR}" ABSOLUTE)
set(_clargs_publish_header "${_clargs_publish_root}/include/clargs/version.h")
set(_clargs_publish_changelog "${_clargs_publish_root}/CHANGELOG.md")
foreach(_clargs_publish_file IN ITEMS "${_clargs_publish_header}" "${_clargs_publish_changelog}")
    if(NOT EXISTS "${_clargs_publish_file}" OR IS_DIRECTORY "${_clargs_publish_file}")
        message(FATAL_ERROR
            "error: ${_clargs_publish_file} does not exist. CLARGS_SOURCE_DIR must be the root "
            "of a source tree with include/clargs/version.h and CHANGELOG.md.")
    endif()
endforeach()

# --- 2. The version --------------------------------------------------------------

clargs_semver_parse("${_clargs_publish_version}" _clargs_publish_v)
if(NOT _clargs_publish_v_VALID)
    message(FATAL_ERROR
        "error: the version to publish is not valid: ${_clargs_publish_v_ERROR}")
endif()
set(_clargs_publish_tag "v${_clargs_publish_version}")

# --- 3. The version header -------------------------------------------------------

# Both versions are valid, so they are equal exactly when their text is.
clargs_read_version("${_clargs_publish_header}" _clargs_publish_header_core
    _clargs_publish_header_prerelease)
set(_clargs_publish_header_version "${_clargs_publish_header_core}")
if(NOT _clargs_publish_header_prerelease STREQUAL "")
    string(APPEND _clargs_publish_header_version "-${_clargs_publish_header_prerelease}")
endif()
if(NOT _clargs_publish_header_version STREQUAL _clargs_publish_version)
    message(FATAL_ERROR
        "error: the version header holds ${_clargs_publish_header_version}, not the version "
        "to publish ${_clargs_publish_version} (${_clargs_publish_header}). The merged commit "
        "does not belong to the release ${_clargs_publish_tag}.")
endif()

# --- 4. The changelog -------------------------------------------------------------

clargs_read_bytes("${_clargs_publish_changelog}" "the changelog" _clargs_publish_changelog_text)
clargs_changelog_parse("${_clargs_publish_changelog_text}" _clargs_publish_c)
if(NOT _clargs_publish_c_VALID)
    message(FATAL_ERROR
        "error: ${_clargs_publish_changelog} is not valid: ${_clargs_publish_c_ERROR}")
endif()
if(_clargs_publish_c_VERSIONS STREQUAL "")
    message(FATAL_ERROR
        "error: the changelog lists no version; its newest version must be the version to "
        "publish ${_clargs_publish_version} (${_clargs_publish_changelog}).")
endif()
list(GET _clargs_publish_c_VERSIONS 0 _clargs_publish_newest)
if(NOT _clargs_publish_newest STREQUAL _clargs_publish_version)
    message(FATAL_ERROR
        "error: the newest version in the changelog is ${_clargs_publish_newest}, not the "
        "version to publish ${_clargs_publish_version} (${_clargs_publish_changelog}). The "
        "merged commit does not belong to the release ${_clargs_publish_tag}.")
endif()

# --- 5. The notes -----------------------------------------------------------------

clargs_changelog_notes("${_clargs_publish_changelog_text}" "${_clargs_publish_version}"
    "${CLARGS_RELEASE_REPO_URL}" _clargs_publish_notes)
string(STRIP "${_clargs_publish_notes}" _clargs_publish_notes_stripped)
if(_clargs_publish_notes_stripped STREQUAL "")
    message(FATAL_ERROR
        "error: the notes of ${_clargs_publish_tag} are empty: the section "
        "[${_clargs_publish_version}] of ${_clargs_publish_changelog} has no text.")
endif()

# The notes have no line ending at their end; they get one of the kind the
# changelog uses.
string(FIND "${_clargs_publish_changelog_text}" "\r\n" _clargs_publish_crlf)
if(_clargs_publish_crlf EQUAL -1)
    string(APPEND _clargs_publish_notes "\n")
else()
    string(APPEND _clargs_publish_notes "\r\n")
endif()

if(_clargs_publish_v_PRERELEASE STREQUAL "")
    set(_clargs_publish_is_prerelease false)
else()
    set(_clargs_publish_is_prerelease true)
endif()

# --- Writing -----------------------------------------------------------------------

get_filename_component(_clargs_publish_notes_file "${CLARGS_RELEASE_NOTES}" ABSOLUTE)
clargs_write_bytes("${_clargs_publish_notes_file}" "${_clargs_publish_notes}" "the notes")

message(STATUS "Release ${_clargs_publish_tag} (pre-release: ${_clargs_publish_is_prerelease}).")
message(STATUS "The version header and the changelog hold ${_clargs_publish_version}.")
message(STATUS "Notes: ${_clargs_publish_notes_file}")

if(DEFINED CLARGS_RELEASE_OUTPUT AND NOT "${CLARGS_RELEASE_OUTPUT}" STREQUAL "")
    file(APPEND "${CLARGS_RELEASE_OUTPUT}"
        "prerelease=${_clargs_publish_is_prerelease}\n"
        "title=${_clargs_publish_tag}\n")
endif()
