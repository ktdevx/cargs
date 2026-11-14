# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# ArgXSemver.cmake - checks a ArgX version and compares two versions by
# SemVer 2.0.0 precedence (https://semver.org/spec/v2.0.0.html).
#
# The release scripts include this module and run with "cmake -P". It needs
# nothing that project() sets, and touches no file, network, git or GitHub.
#
#   argx_semver_parse(<version> <prefix>)
#
# Checks that <version> is of the form
#
#   <major>.<minor>.<patch>[-<pre-release>]
#
# and sets, in the scope of the caller:
#
#   <prefix>_VALID       TRUE or FALSE.
#   <prefix>_ERROR       "" when valid; otherwise one or more English sentences
#                        that name the version and every rule it breaks.
#   <prefix>_MAJOR, <prefix>_MINOR, <prefix>_PATCH
#                        The numbers, as decimal strings.
#   <prefix>_PRERELEASE  The pre-release identifiers without the leading
#                        hyphen ("rc.1"), or "" for a release.
#   <prefix>_CORE        "<major>.<minor>.<patch>".
#
# When the version is not valid, every variable other than <prefix>_VALID and
# <prefix>_ERROR is "". A version is valid when:
#
#   - It has no build metadata ("+..."). SemVer allows it, but ArgX does not
#     use it, since it plays no part in precedence and a tag must name one
#     version.
#   - Each of major, minor and patch is a decimal number without leading zeros
#     in the range 0-255. The range is a limit of the version header
#     (include/argx/version.h), not of SemVer.
#   - The pre-release, when there is a hyphen, is one or more identifiers
#     separated by dots, each made of [0-9A-Za-z-] and not empty, and none that
#     is all digits starting with 0 (other than "0" itself).
#
#   argx_semver_compare(<a> <b> <out-var>)
#
# Sets <out-var> to -1, 0 or 1 when <a> is lower than, equal to or higher than
# <b> by SemVer 2.0.0 precedence:
#
#   1. Major, minor and patch compare as numbers, in that order.
#   2. With the same core, a pre-release is lower than the release.
#   3. Two pre-releases compare identifier by identifier, from the left. Two
#      numeric identifiers compare as numbers; an identifier of digits only is
#      lower than one with other characters; two other identifiers compare in
#      ASCII order.
#   4. When every identifier of the shorter list equals the one in the same
#      place of the other, the shorter list is lower.
#
# Numeric identifiers may be longer than any integer CMake computes with, so
# they are compared by their number of digits and then as text; no arithmetic
# is done on them.
#
# Stops with a FATAL_ERROR that starts with "error: " and holds the reason
# when <a> or <b> is not valid.

include_guard(GLOBAL)

# The functions keep the policies in effect here, whatever the caller has set
# (a "cmake -P" script may have set none).
cmake_policy(PUSH)
cmake_policy(VERSION 3.21)

function(argx_semver_parse version prefix)
    set(problems "")
    set(major "")
    set(minor "")
    set(patch "")
    set(prerelease "")

    string(FIND "${version}" "+" plus_pos)
    if(NOT plus_pos EQUAL -1)
        string(APPEND problems " build metadata (\"+...\") is not accepted;")
    elseif(NOT version MATCHES "^([0-9]+)\\.([0-9]+)\\.([0-9]+)(-(.*))?$")
        string(APPEND problems
            " it is not of the form <major>.<minor>.<patch>[-<pre-release>];")
    else()
        set(major "${CMAKE_MATCH_1}")
        set(minor "${CMAKE_MATCH_2}")
        set(patch "${CMAKE_MATCH_3}")
        # An optional group that did not match may keep the value of an earlier
        # match in CMAKE_MATCH_<n>, so the hyphen is looked for in the text. The
        # core holds no hyphen: the first one starts the pre-release.
        set(has_hyphen FALSE)
        string(FIND "${version}" "-" hyphen_pos)
        if(NOT hyphen_pos EQUAL -1)
            set(has_hyphen TRUE)
            math(EXPR after "${hyphen_pos} + 1")
            string(SUBSTRING "${version}" ${after} -1 prerelease)
        endif()

        foreach(part IN ITEMS major minor patch)
            set(value "${${part}}")
            string(LENGTH "${value}" digits)
            if(value MATCHES "^0[0-9]")
                string(APPEND problems " the ${part} version \"${value}\" has a leading zero;")
            elseif(digits GREATER 3)
                # Checked apart from the value, so that no number too large for
                # CMake is ever compared as a number.
                string(APPEND problems
                    " the ${part} version \"${value}\" is not in the range 0-255;")
            elseif(value GREATER 255)
                string(APPEND problems
                    " the ${part} version \"${value}\" is not in the range 0-255;")
            endif()
        endforeach()

        if(has_hyphen)
            if(prerelease STREQUAL "")
                string(APPEND problems
                    " the pre-release identifiers are empty after the hyphen;")
            elseif(prerelease MATCHES "(^|\\.)(\\.|$)")
                string(APPEND problems " it has an empty pre-release identifier;")
            elseif(NOT prerelease MATCHES "^[0-9A-Za-z-]+(\\.[0-9A-Za-z-]+)*$")
                string(APPEND problems
                    " \"${prerelease}\" are not valid pre-release identifiers"
                    " (each must be made of [0-9A-Za-z-]);")
            else()
                # The characters exclude ";", so the identifiers make a list.
                string(REPLACE "." ";" identifiers "${prerelease}")
                foreach(identifier IN LISTS identifiers)
                    if(identifier MATCHES "^0[0-9]+$")
                        string(APPEND problems
                            " the numeric pre-release identifier \"${identifier}\""
                            " has a leading zero;")
                    endif()
                endforeach()
            endif()
        endif()
    endif()

    if(problems STREQUAL "")
        set(${prefix}_VALID TRUE PARENT_SCOPE)
        set(${prefix}_ERROR "" PARENT_SCOPE)
        set(${prefix}_MAJOR "${major}" PARENT_SCOPE)
        set(${prefix}_MINOR "${minor}" PARENT_SCOPE)
        set(${prefix}_PATCH "${patch}" PARENT_SCOPE)
        set(${prefix}_PRERELEASE "${prerelease}" PARENT_SCOPE)
        set(${prefix}_CORE "${major}.${minor}.${patch}" PARENT_SCOPE)
    else()
        # Each problem ends with ";": the last becomes the full stop of the
        # sentence, the others separate the problems.
        string(REGEX REPLACE ";$" "." problems "${problems}")
        set(${prefix}_VALID FALSE PARENT_SCOPE)
        set(${prefix}_ERROR
            "\"${version}\" is not a valid version:${problems} A version must be of the form \
<major>.<minor>.<patch>[-<pre-release>], such as 1.2.3 or 1.2.3-rc.1, \
with each number in the range 0-255 and without leading zeros."
            PARENT_SCOPE)
        foreach(field IN ITEMS MAJOR MINOR PATCH PRERELEASE CORE)
            set(${prefix}_${field} "" PARENT_SCOPE)
        endforeach()
    endif()
endfunction()

# Sets <out-var> to -1, 0 or 1 as the identifier <a> is lower than, equal to or
# higher than the identifier <b>. Both are valid pre-release identifiers.
function(_argx_semver_compare_identifier a b out_var)
    set(a_numeric FALSE)
    set(b_numeric FALSE)
    if(a MATCHES "^[0-9]+$")
        set(a_numeric TRUE)
    endif()
    if(b MATCHES "^[0-9]+$")
        set(b_numeric TRUE)
    endif()

    if("${a}" STREQUAL "${b}")
        set(result 0)
    elseif(a_numeric AND NOT b_numeric)
        set(result -1)
    elseif(b_numeric AND NOT a_numeric)
        set(result 1)
    else()
        set(result 0)
        if(a_numeric)
            # Without leading zeros, more digits mean a larger number; with the
            # same number of digits, the text order is the numeric order.
            string(LENGTH "${a}" a_length)
            string(LENGTH "${b}" b_length)
            if(a_length LESS b_length)
                set(result -1)
            elseif(a_length GREATER b_length)
                set(result 1)
            endif()
        endif()
        if(result EQUAL 0)
            if("${a}" STRLESS "${b}")
                set(result -1)
            else()
                set(result 1)
            endif()
        endif()
    endif()
    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

function(argx_semver_compare a b out_var)
    argx_semver_parse("${a}" a)
    argx_semver_parse("${b}" b)
    foreach(side IN ITEMS a b)
        if(NOT ${side}_VALID)
            message(FATAL_ERROR "error: cannot compare the versions: ${${side}_ERROR}")
        endif()
    endforeach()

    set(result 0)
    foreach(part IN ITEMS MAJOR MINOR PATCH)
        if(result EQUAL 0)
            # Both are at most 255 here, so they compare safely as numbers.
            if(a_${part} LESS b_${part})
                set(result -1)
            elseif(a_${part} GREATER b_${part})
                set(result 1)
            endif()
        endif()
    endforeach()

    if(result EQUAL 0)
        if(a_PRERELEASE STREQUAL "" AND b_PRERELEASE STREQUAL "")
            set(result 0)
        elseif(a_PRERELEASE STREQUAL "")
            set(result 1)
        elseif(b_PRERELEASE STREQUAL "")
            set(result -1)
        else()
            # Valid identifiers are never empty and hold no ";", so the lists
            # keep every identifier.
            string(REPLACE "." ";" a_identifiers "${a_PRERELEASE}")
            string(REPLACE "." ";" b_identifiers "${b_PRERELEASE}")
            list(LENGTH a_identifiers a_count)
            list(LENGTH b_identifiers b_count)
            set(common ${a_count})
            if(b_count LESS common)
                set(common ${b_count})
            endif()
            math(EXPR last "${common} - 1")
            foreach(index RANGE ${last})
                if(result EQUAL 0)
                    list(GET a_identifiers ${index} a_identifier)
                    list(GET b_identifiers ${index} b_identifier)
                    _argx_semver_compare_identifier("${a_identifier}" "${b_identifier}" result)
                endif()
            endforeach()
            if(result EQUAL 0)
                if(a_count LESS b_count)
                    set(result -1)
                elseif(a_count GREATER b_count)
                    set(result 1)
                endif()
            endif()
        endif()
    endif()

    set(${out_var} ${result} PARENT_SCOPE)
endfunction()

cmake_policy(POP)
