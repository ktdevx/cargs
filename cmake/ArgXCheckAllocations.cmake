# ArgXCheckAllocations.cmake - checks that the library sources do not use
# the allocation functions of the C standard library directly.
#
# A script, run with "cmake -P". It needs no build and runs by hand:
#
#   cmake [-D ARGX_SOURCE_DIR=<directory>] -P cmake/ArgXCheckAllocations.cmake
#
#   ARGX_SOURCE_DIR  The directory of the library sources. Default: src of
#                    this source tree.
#
# The rules:
#
#   - The files checked are all the *.c and *.h files of ARGX_SOURCE_DIR,
#     its subdirectories included, except alloc.c in ARGX_SOURCE_DIR itself.
#     alloc.c holds the default allocator, the one place that may call the
#     standard functions. Every other file, alloc_internal.h included, is
#     checked.
#   - None of these names appears as a whole identifier in a checked file:
#     malloc, calloc, realloc, free, aligned_alloc, strdup, strndup, wcsdup,
#     asprintf, vasprintf. Library code allocates with argx_malloc,
#     argx_calloc, argx_realloc and argx_free instead, so that every
#     allocation goes through the allocator that the user has set.
#   - Comments, string literals and character literals are not checked.
#     Everything else is, preprocessor lines included.
#   - There is at least one file to check. Finding none means that
#     ARGX_SOURCE_DIR or this script is wrong.
#
# How a file is read: the comments and the literals are blanked out, and the
# words that are left are compared with the names above. So argx_malloc,
# malloc_fn and freed are fine, and "free" fails wherever it stands: called,
# or used as a value. A quote with no closing quote on its line (an
# apostrophe in an #error line, say) does not start a literal, and the text
# after it is checked. The check reads names, it does not preprocess: it does
# not see a name that a macro pastes together, nor one that is split by a
# backslash at the end of a line.
#
# Ends with a FATAL_ERROR that lists the file, the line number and the name
# of every use, so the test fails. Otherwise prints the number of files
# checked. Reads the files only, and changes none.

cmake_minimum_required(VERSION 3.21)

# The names that only alloc.c may use.
set(argx_check_forbidden_names
    malloc calloc realloc free aligned_alloc
    strdup strndup wcsdup asprintf vasprintf)

# argx_check_blank_code(<text> <out-var>)
#
# Replaces each comment, string literal and character literal of the C code
# <text> with a blank. The line breaks inside them are kept, so every line of
# the result has the line number it has in <text>. <text> has "\n" line ends.
function(argx_check_blank_code rest out_var)
    set(text "")
    while(TRUE)
        # The next place where a comment or a literal starts.
        string(REGEX MATCH "/[*/]|[\"']" token "${rest}")
        if(token STREQUAL "")
            break()
        endif()
        string(FIND "${rest}" "${token}" pos)
        string(LENGTH "${token}" length)
        string(SUBSTRING "${rest}" 0 ${pos} head)
        math(EXPR after "${pos} + ${length}")
        string(SUBSTRING "${rest}" ${after} -1 rest)
        string(APPEND text "${head}")

        if(token STREQUAL "/*")
            # A block comment: up to the first "*/", or to the end of the file.
            string(FIND "${rest}" "*/" end_pos)
            if(end_pos EQUAL -1)
                set(body "${rest}")
                set(rest "")
            else()
                string(SUBSTRING "${rest}" 0 ${end_pos} body)
                math(EXPR after "${end_pos} + 2")
                string(SUBSTRING "${rest}" ${after} -1 rest)
            endif()
            string(REGEX REPLACE "[^\n]+" "" breaks "${body}")
            string(APPEND text " ${breaks}")

        elseif(token STREQUAL "//")
            # A line comment: up to the end of the line, and on into the next
            # line while the line ends with a backslash.
            string(APPEND text " ")
            while(TRUE)
                string(FIND "${rest}" "\n" end_pos)
                if(end_pos EQUAL -1)
                    set(rest "")
                    break()
                endif()
                set(last "")
                if(end_pos GREATER 0)
                    math(EXPR last_pos "${end_pos} - 1")
                    string(SUBSTRING "${rest}" ${last_pos} 1 last)
                endif()
                if(NOT last STREQUAL "\\")
                    string(SUBSTRING "${rest}" ${end_pos} -1 rest)
                    break()
                endif()
                math(EXPR after "${end_pos} + 1")
                string(SUBSTRING "${rest}" ${after} -1 rest)
                string(APPEND text "\n")
            endwhile()

        else()
            # A string or character literal: up to the same quote again. A
            # backslash takes the character after it along, be it a quote, a
            # backslash or the end of the line.
            set(scan "${rest}")
            set(breaks "")
            set(closed FALSE)
            while(TRUE)
                string(REGEX MATCH "[${token}\\\\\n]" stop "${scan}")
                if(stop STREQUAL "" OR stop STREQUAL "\n")
                    break()
                endif()
                string(FIND "${scan}" "${stop}" stop_pos)
                if(stop STREQUAL token)
                    math(EXPR after "${stop_pos} + 1")
                    string(SUBSTRING "${scan}" ${after} -1 scan)
                    set(closed TRUE)
                    break()
                endif()
                math(EXPR escaped_pos "${stop_pos} + 1")
                string(SUBSTRING "${scan}" ${escaped_pos} 1 escaped)
                if(escaped STREQUAL "")
                    break()
                elseif(escaped STREQUAL "\n")
                    string(APPEND breaks "\n")
                endif()
                math(EXPR after "${stop_pos} + 2")
                string(SUBSTRING "${scan}" ${after} -1 scan)
            endwhile()
            if(closed)
                set(rest "${scan}")
                string(APPEND text " ${breaks}")
            else()
                # No closing quote on the line: not a literal. Only the quote
                # is dropped, and the text after it is read as code.
                string(APPEND text " ")
            endif()
        endif()
    endwhile()
    set(${out_var} "${text}${rest}" PARENT_SCOPE)
endfunction()

# argx_check_read_source(<file> <name> <uses-var>)
#
# Appends a line "  <name>:<line number>: <forbidden name>" to <uses-var> for
# each use of a forbidden name in <file>.
function(argx_check_read_source file name uses_var)
    set(uses "${${uses_var}}")

    file(READ "${file}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REPLACE "\r" "\n" text "${text}")
    argx_check_blank_code("${text}" text)

    # Most files hold none of the names at all.
    list(JOIN argx_check_forbidden_names "|" any_name)
    if(NOT text MATCHES "${any_name}")
        return()
    endif()

    # One list element for each line. A ";", a bracket and a backslash are
    # not part of a name, and they would break the list.
    string(REGEX REPLACE "[][;\\\\]" " " text "${text}")
    string(REPLACE "\n" ";" lines "${text}")

    set(line_number 0)
    foreach(line IN LISTS lines)
        math(EXPR line_number "${line_number} + 1")
        if(NOT line MATCHES "${any_name}")
            continue()
        endif()
        # A word is a whole identifier (or a number), so "argx_malloc" and
        # "freed" are words of their own and are not in the list.
        string(REGEX MATCHALL "[A-Za-z0-9_]+" words "${line}")
        foreach(word IN LISTS words)
            if(word IN_LIST argx_check_forbidden_names)
                string(APPEND uses "  ${name}:${line_number}: ${word}\n")
            endif()
        endforeach()
    endforeach()

    set(${uses_var} "${uses}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# The arguments.
# ---------------------------------------------------------------------------

if(NOT DEFINED ARGX_SOURCE_DIR OR ARGX_SOURCE_DIR STREQUAL "")
    set(ARGX_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../src")
endif()
get_filename_component(ARGX_SOURCE_DIR "${ARGX_SOURCE_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${ARGX_SOURCE_DIR}")
    message(FATAL_ERROR
        "argx.allocations: the directory of the library sources does not exist:\n"
        "  ${ARGX_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------------------
# The files to check.
# ---------------------------------------------------------------------------

file(GLOB_RECURSE argx_check_sources
    LIST_DIRECTORIES false
    RELATIVE "${ARGX_SOURCE_DIR}"
    "${ARGX_SOURCE_DIR}/*.c" "${ARGX_SOURCE_DIR}/*.h")
list(REMOVE_ITEM argx_check_sources "alloc.c")
list(SORT argx_check_sources)
list(LENGTH argx_check_sources argx_check_count)
if(argx_check_count EQUAL 0)
    message(FATAL_ERROR
        "argx.allocations: no file to check (*.c or *.h, other than alloc.c) "
        "was found in\n"
        "  ${ARGX_SOURCE_DIR}")
endif()

# ---------------------------------------------------------------------------
# The check.
# ---------------------------------------------------------------------------

set(argx_check_uses "")
foreach(argx_check_source IN LISTS argx_check_sources)
    argx_check_read_source("${ARGX_SOURCE_DIR}/${argx_check_source}"
        "${argx_check_source}" argx_check_uses)
endforeach()

if(NOT argx_check_uses STREQUAL "")
    list(JOIN argx_check_forbidden_names " " argx_check_names_text)
    message(FATAL_ERROR
        "argx.allocations: the library sources use allocation functions of "
        "the C standard library directly (file:line: name).\n"
        "${argx_check_uses}"
        "in\n"
        "  ${ARGX_SOURCE_DIR}\n"
        "Only alloc.c may use these names: ${argx_check_names_text}\n"
        "Use argx_malloc, argx_calloc, argx_realloc and argx_free "
        "instead.")
endif()

message(STATUS
    "argx.allocations: ${argx_check_count} files checked in "
    "${ARGX_SOURCE_DIR}: none uses an allocation function of the C standard "
    "library directly.")
