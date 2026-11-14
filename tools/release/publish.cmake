# publish.cmake - checks the commit of a merged release pull request against
# the version of the release, and makes the notes of its GitHub Release.
#
#   cmake -D ARGX_RELEASE_VERSION=<version> -D ARGX_RELEASE_REPO_URL=<url>
#         -D ARGX_SOURCE_DIR=<dir> -D ARGX_RELEASE_NOTES=<file>
#         [-D ARGX_RELEASE_OUTPUT=<file>] -P tools/release/publish.cmake
#
#   ARGX_RELEASE_VERSION  The version to publish, such as 0.2.0 or
#                         0.2.0-rc.1, without a leading "v" (the release
#                         workflow takes it from the name of the branch
#                         release/v<version>).
#   ARGX_RELEASE_REPO_URL https://github.com/<owner>/<repo>, for the links to
#                         earlier pre-releases in the notes.
#   ARGX_SOURCE_DIR       The root of the source tree of the merged commit: it
#                         holds include/argx/version.h and CHANGELOG.md.
#   ARGX_RELEASE_NOTES    The file to write the notes of the release to. Its
#                         directory is made when it does not exist.
#   ARGX_RELEASE_OUTPUT   Optional. A file to append the results to, one
#                         "key=value" line each, such as $GITHUB_OUTPUT:
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
#   2. The version is valid for ArgXSemver.cmake.
#   3. The version header holds the version, pre-release included, as
#      argx_read_version() reads it.
#   4. The changelog is valid for ArgXChangelog.cmake and its newest version
#      is the version.
#   5. The notes are not empty.
#
# The notes are the section of the version in the changelog, without its
# heading, as argx_changelog_notes() gives them: for a release without a
# pre-release, a sentence that refers to the pre-releases of the same
# <major>.<minor>.<patch> right below it follows the section. The changelog is
# read and the notes are written byte for byte (ArgXReadBytes.cmake), so the
# notes keep its line endings (LF or CRLF), on Windows too, and they end with
# one.
#
# A version with a pre-release, such as 0.2.0-rc.1, gives prerelease=true; the
# release workflow then marks the GitHub Release as a pre-release and not as
# the latest release. The script reads the two files of the tree and writes
# only the notes and the output file; it touches no network, git or GitHub.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ArgXSemver.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ArgXChangelog.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/ArgXReadBytes.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/ArgXVersion.cmake")

# --- 1. The inputs -------------------------------------------------------------

set(_argx_publish_problems "")
foreach(_argx_publish_input IN ITEMS ARGX_RELEASE_VERSION ARGX_RELEASE_REPO_URL
        ARGX_SOURCE_DIR ARGX_RELEASE_NOTES)
    if(NOT DEFINED ${_argx_publish_input} OR "${${_argx_publish_input}}" STREQUAL "")
        string(APPEND _argx_publish_problems "\n  ${_argx_publish_input} is not set.")
    endif()
endforeach()
if(NOT _argx_publish_problems STREQUAL "")
    message(FATAL_ERROR
        "error: a required input is missing:${_argx_publish_problems}\n"
        "Run the script as\n"
        "  cmake -D ARGX_RELEASE_VERSION=<version> -D ARGX_RELEASE_REPO_URL=<url> "
        "-D ARGX_SOURCE_DIR=<dir> -D ARGX_RELEASE_NOTES=<file> "
        "[-D ARGX_RELEASE_OUTPUT=<file>] -P tools/release/publish.cmake")
endif()

set(_argx_publish_version "${ARGX_RELEASE_VERSION}")
get_filename_component(_argx_publish_root "${ARGX_SOURCE_DIR}" ABSOLUTE)
set(_argx_publish_header "${_argx_publish_root}/include/argx/version.h")
set(_argx_publish_changelog "${_argx_publish_root}/CHANGELOG.md")
foreach(_argx_publish_file IN ITEMS "${_argx_publish_header}" "${_argx_publish_changelog}")
    if(NOT EXISTS "${_argx_publish_file}" OR IS_DIRECTORY "${_argx_publish_file}")
        message(FATAL_ERROR
            "error: ${_argx_publish_file} does not exist. ARGX_SOURCE_DIR must be the root "
            "of a source tree with include/argx/version.h and CHANGELOG.md.")
    endif()
endforeach()

# --- 2. The version --------------------------------------------------------------

argx_semver_parse("${_argx_publish_version}" _argx_publish_v)
if(NOT _argx_publish_v_VALID)
    message(FATAL_ERROR
        "error: the version to publish is not valid: ${_argx_publish_v_ERROR}")
endif()
set(_argx_publish_tag "v${_argx_publish_version}")

# --- 3. The version header -------------------------------------------------------

# Both versions are valid, so they are equal exactly when their text is.
argx_read_version("${_argx_publish_header}" _argx_publish_header_core
    _argx_publish_header_prerelease)
set(_argx_publish_header_version "${_argx_publish_header_core}")
if(NOT _argx_publish_header_prerelease STREQUAL "")
    string(APPEND _argx_publish_header_version "-${_argx_publish_header_prerelease}")
endif()
if(NOT _argx_publish_header_version STREQUAL _argx_publish_version)
    message(FATAL_ERROR
        "error: the version header holds ${_argx_publish_header_version}, not the version "
        "to publish ${_argx_publish_version} (${_argx_publish_header}). The merged commit "
        "does not belong to the release ${_argx_publish_tag}.")
endif()

# --- 4. The changelog -------------------------------------------------------------

argx_read_bytes("${_argx_publish_changelog}" "the changelog" _argx_publish_changelog_text)
argx_changelog_parse("${_argx_publish_changelog_text}" _argx_publish_c)
if(NOT _argx_publish_c_VALID)
    message(FATAL_ERROR
        "error: ${_argx_publish_changelog} is not valid: ${_argx_publish_c_ERROR}")
endif()
if(_argx_publish_c_VERSIONS STREQUAL "")
    message(FATAL_ERROR
        "error: the changelog lists no version; its newest version must be the version to "
        "publish ${_argx_publish_version} (${_argx_publish_changelog}).")
endif()
list(GET _argx_publish_c_VERSIONS 0 _argx_publish_newest)
if(NOT _argx_publish_newest STREQUAL _argx_publish_version)
    message(FATAL_ERROR
        "error: the newest version in the changelog is ${_argx_publish_newest}, not the "
        "version to publish ${_argx_publish_version} (${_argx_publish_changelog}). The "
        "merged commit does not belong to the release ${_argx_publish_tag}.")
endif()

# --- 5. The notes -----------------------------------------------------------------

argx_changelog_notes("${_argx_publish_changelog_text}" "${_argx_publish_version}"
    "${ARGX_RELEASE_REPO_URL}" _argx_publish_notes)
string(STRIP "${_argx_publish_notes}" _argx_publish_notes_stripped)
if(_argx_publish_notes_stripped STREQUAL "")
    message(FATAL_ERROR
        "error: the notes of ${_argx_publish_tag} are empty: the section "
        "[${_argx_publish_version}] of ${_argx_publish_changelog} has no text.")
endif()

# The notes have no line ending at their end; they get one of the kind the
# changelog uses.
string(FIND "${_argx_publish_changelog_text}" "\r\n" _argx_publish_crlf)
if(_argx_publish_crlf EQUAL -1)
    string(APPEND _argx_publish_notes "\n")
else()
    string(APPEND _argx_publish_notes "\r\n")
endif()

if(_argx_publish_v_PRERELEASE STREQUAL "")
    set(_argx_publish_is_prerelease false)
else()
    set(_argx_publish_is_prerelease true)
endif()

# --- Writing -----------------------------------------------------------------------

get_filename_component(_argx_publish_notes_file "${ARGX_RELEASE_NOTES}" ABSOLUTE)
argx_write_bytes("${_argx_publish_notes_file}" "${_argx_publish_notes}" "the notes")

message(STATUS "Release ${_argx_publish_tag} (pre-release: ${_argx_publish_is_prerelease}).")
message(STATUS "The version header and the changelog hold ${_argx_publish_version}.")
message(STATUS "Notes: ${_argx_publish_notes_file}")

if(DEFINED ARGX_RELEASE_OUTPUT AND NOT "${ARGX_RELEASE_OUTPUT}" STREQUAL "")
    file(APPEND "${ARGX_RELEASE_OUTPUT}"
        "prerelease=${_argx_publish_is_prerelease}\n"
        "title=${_argx_publish_tag}\n")
endif()
