# ClineCheckExports.cmake - checks what the shared library exports.
#
# A script, run with "cmake -P". When the library is a shared library and the
# tests are built, tests/CMakeLists.txt registers it with CTest under the name
# cline.exports. It also runs by hand:
#
#   cmake -D CLINE_LIBRARY=<shared library> [-D <variable>=<value>]...
#         -P cmake/ClineCheckExports.cmake
#
#   CLINE_LIBRARY      The shared library to check: the .so, the .dylib or
#                       the .dll (not the import library). Required.
#   CLINE_INCLUDE_DIR  The directory of the public headers. Default:
#                       include/cline of this source tree.
#   CLINE_FORMAT       ELF, MACHO or PE. Default: the format of the machine
#                       the script runs on (Windows: PE, macOS: MACHO,
#                       otherwise ELF).
#   CLINE_TOOL         The program that lists the exported symbols: nm for
#                       ELF and MACHO, dumpbin for PE. Default: looked up on
#                       the PATH.
#
# The rules:
#
#   - The expected names come from the public headers, all the *.h files of
#     CLINE_INCLUDE_DIR: every function declared with CLINE_API. Nothing
#     here lists the public functions by hand, so a new public function needs
#     no change to this script.
#   - The library exports exactly the expected names. A symbol that is
#     exported but not declared with CLINE_API in a public header fails the
#     check, and so does a function that is declared with CLINE_API but not
#     exported.
#   - Every expected name starts with "cline_".
#   - The public headers declare at least one function with CLINE_API.
#     Finding none means that the headers or this script are broken.
#
# How a declaration is read: comments and preprocessor lines are dropped, and
# the name is the last identifier between CLINE_API and the first "(" after
# it. So a public declaration is written in this order: CLINE_API, the return
# type, the function name, "(". CLINE_API on anything else (a variable, a
# function that returns a function pointer without a typedef) fails the check.
#
# Ends with a FATAL_ERROR that names every symbol at fault, so the test fails.
# A missing library, a missing tool and a tool that fails are errors too.

cmake_minimum_required(VERSION 3.21)

# Symbols that the linker or the C runtime puts into every ELF shared library
# with some toolchains. They are not symbols of CLine.
set(cline_check_toolchain_symbols _init _fini __bss_start _edata _end)

# cline_check_strip_comments(<text> <out-var>)
#
# Drops the C comments of <text>. A block comment becomes one blank, a line
# comment ends before the end of its line.
function(cline_check_strip_comments rest out_var)
    set(text "")
    while(TRUE)
        string(FIND "${rest}" "/*" block_pos)
        string(FIND "${rest}" "//" line_pos)
        if(block_pos EQUAL -1 AND line_pos EQUAL -1)
            break()
        endif()
        if(line_pos EQUAL -1 OR (NOT block_pos EQUAL -1 AND block_pos LESS line_pos))
            string(SUBSTRING "${rest}" 0 ${block_pos} head)
            math(EXPR after "${block_pos} + 2")
            string(SUBSTRING "${rest}" ${after} -1 rest)
            string(FIND "${rest}" "*/" end_pos)
            if(end_pos EQUAL -1)
                set(rest "")
            else()
                math(EXPR after "${end_pos} + 2")
                string(SUBSTRING "${rest}" ${after} -1 rest)
            endif()
            string(APPEND text "${head} ")
        else()
            string(SUBSTRING "${rest}" 0 ${line_pos} head)
            string(SUBSTRING "${rest}" ${line_pos} -1 rest)
            string(FIND "${rest}" "\n" end_pos)
            if(end_pos EQUAL -1)
                set(rest "")
            else()
                string(SUBSTRING "${rest}" ${end_pos} -1 rest)
            endif()
            string(APPEND text "${head}")
        endif()
    endwhile()
    set(${out_var} "${text}${rest}" PARENT_SCOPE)
endfunction()

# cline_check_read_header(<header> <names-var> <problems-var>)
#
# Appends the names of the functions that <header> declares with CLINE_API
# to <names-var>, and a line for each use of CLINE_API that is not such a
# declaration to <problems-var>.
function(cline_check_read_header header names_var problems_var)
    set(names "${${names_var}}")
    set(problems "${${problems_var}}")

    file(READ "${header}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(REPLACE "\r" "\n" text "${text}")

    # As the compiler does: join the continued lines, then drop the comments.
    string(REPLACE "\\\n" "" text "${text}")
    cline_check_strip_comments("${text}" text)

    # Drop the preprocessor lines. The definitions of CLINE_API itself are
    # among them.
    string(REGEX REPLACE "(^|\n)[ \t]*#[^\n]*" "\\1" text "${text}")

    while(TRUE)
        string(FIND "${text}" "CLINE_API" pos)
        if(pos EQUAL -1)
            break()
        endif()

        # The character before and the text after this use of CLINE_API.
        set(before " ")
        if(pos GREATER 0)
            math(EXPR before_pos "${pos} - 1")
            string(SUBSTRING "${text}" ${before_pos} 1 before)
        endif()
        math(EXPR after "${pos} + 10")
        string(SUBSTRING "${text}" ${after} -1 text)

        # Part of a longer identifier: not the marker.
        if(before MATCHES "[A-Za-z0-9_]" OR text MATCHES "^[A-Za-z0-9_]")
            continue()
        endif()

        # The declaration up to its first "(". One that ends first is not the
        # declaration of a function.
        if(NOT text MATCHES "^([^(;{}]*)\\(")
            string(REGEX MATCH "^[^;{}]*" what "${text}")
            string(REGEX REPLACE "[ \t\n]+" " " what "${what}")
            string(STRIP "${what}" what)
            string(APPEND problems
                "  ${header}: CLINE_API marks \"${what}\", which is not the "
                "declaration of a function.\n")
            continue()
        endif()
        set(declaration "${CMAKE_MATCH_1}")

        if(NOT declaration MATCHES "([A-Za-z_][A-Za-z0-9_]*)[ \t\n]*$")
            string(REGEX REPLACE "[ \t\n]+" " " what "${declaration}")
            string(STRIP "${what}" what)
            string(APPEND problems
                "  ${header}: cannot read the function name in "
                "\"CLINE_API ${what}(\". Write CLINE_API, the return type, "
                "the function name and \"(\" in this order.\n")
            continue()
        endif()
        list(APPEND names "${CMAKE_MATCH_1}")
    endwhile()

    set(${names_var} "${names}" PARENT_SCOPE)
    set(${problems_var} "${problems}" PARENT_SCOPE)
endfunction()

# cline_check_list_exports(<out-var>)
#
# Sets <out-var> to the names that CLINE_LIBRARY exports, as the C code
# spells them. Stops with a FATAL_ERROR if the tool fails or its output
# cannot be read.
function(cline_check_list_exports out_var)
    if(CLINE_FORMAT STREQUAL "PE")
        set(arguments /NOLOGO /EXPORTS)
    elseif(CLINE_FORMAT STREQUAL "MACHO")
        # The external symbols that the library defines.
        set(arguments -gU)
    else()
        # The dynamic symbols that the library defines.
        set(arguments -D --defined-only)
    endif()

    # The tools print in English, whatever the user has set.
    set(ENV{LC_ALL} C)
    set(ENV{VSLANG} 1033)
    execute_process(
        COMMAND "${CLINE_TOOL}" ${arguments} "${CLINE_LIBRARY}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
    )
    list(JOIN arguments " " arguments_text)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR
            "cline.exports: cannot list the exported symbols of\n"
            "  ${CLINE_LIBRARY}\n"
            "The command\n"
            "  ${CLINE_TOOL} ${arguments_text} <library>\n"
            "failed (${result}):\n"
            "${output}${errors}")
    endif()

    string(REPLACE "\r\n" "\n" output "${output}")
    string(REPLACE "\r" "\n" output "${output}")
    # One list element for each line. No symbol name of C holds a ";" or a
    # bracket, so a line that does is not one this function reads.
    string(REGEX REPLACE "[][;\\\\]" "?" output "${output}")
    string(REPLACE "\n" ";" lines "${output}")

    set(names "")
    if(CLINE_FORMAT STREQUAL "PE")
        # dumpbin prints one line for each export:
        #     ordinal hint RVA      name
        #           1    0 00001000 cline_version
        #           1    0 00001000 cline_version = @ILT+0(cline_version)
        # and, before them, "<count> number of names".
        set(hex "[0-9A-Fa-f]")
        set(rva "${hex}${hex}${hex}${hex}${hex}${hex}${hex}${hex}+")
        set(count "")
        foreach(line IN LISTS lines)
            if(line MATCHES "^[ \t]+([0-9]+)[ \t]+number of names[ \t]*$")
                set(count "${CMAKE_MATCH_1}")
            elseif(line MATCHES "^[ \t]+[0-9]+[ \t]+${hex}+[ \t]+${rva}[ \t]+([^ \t]+)")
                list(APPEND names "${CMAKE_MATCH_1}")
            endif()
        endforeach()
        list(LENGTH names found)
        if(NOT count STREQUAL "" AND NOT count EQUAL found)
            message(FATAL_ERROR
                "cline.exports: cannot read the output of\n"
                "  ${CLINE_TOOL} ${arguments_text} ${CLINE_LIBRARY}\n"
                "It announces ${count} exported names, and ${found} were read:\n"
                "${output}")
        endif()
    else()
        # nm prints one line for each symbol: "<address> <type> <name>".
        foreach(line IN LISTS lines)
            if(NOT line MATCHES "^[0-9A-Fa-f]+[ \t]+[A-Za-z][ \t]+([^ \t]+)[ \t]*$")
                continue()
            endif()
            set(name "${CMAKE_MATCH_1}")
            if(CLINE_FORMAT STREQUAL "MACHO")
                # The C name follows one underscore.
                string(REGEX REPLACE "^_" "" name "${name}")
            else()
                # A versioned symbol is "<name>@@<version>".
                string(REGEX REPLACE "@.*$" "" name "${name}")
                if(name IN_LIST cline_check_toolchain_symbols)
                    continue()
                endif()
            endif()
            list(APPEND names "${name}")
        endforeach()
    endif()

    list(REMOVE_DUPLICATES names)
    list(SORT names)
    set(${out_var} "${names}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# The arguments.
# ---------------------------------------------------------------------------

if(NOT DEFINED CLINE_LIBRARY OR CLINE_LIBRARY STREQUAL "")
    message(FATAL_ERROR
        "cline.exports: CLINE_LIBRARY is not set. Usage:\n"
        "  cmake -D CLINE_LIBRARY=<shared library> -P ${CMAKE_CURRENT_LIST_FILE}")
endif()
get_filename_component(CLINE_LIBRARY "${CLINE_LIBRARY}" ABSOLUTE)
if(NOT EXISTS "${CLINE_LIBRARY}" OR IS_DIRECTORY "${CLINE_LIBRARY}")
    message(FATAL_ERROR
        "cline.exports: the library to check does not exist:\n"
        "  ${CLINE_LIBRARY}\n"
        "Build the library first.")
endif()

if(NOT DEFINED CLINE_INCLUDE_DIR OR CLINE_INCLUDE_DIR STREQUAL "")
    set(CLINE_INCLUDE_DIR "${CMAKE_CURRENT_LIST_DIR}/../include/cline")
endif()
get_filename_component(CLINE_INCLUDE_DIR "${CLINE_INCLUDE_DIR}" ABSOLUTE)
if(NOT IS_DIRECTORY "${CLINE_INCLUDE_DIR}")
    message(FATAL_ERROR
        "cline.exports: the directory of the public headers does not exist:\n"
        "  ${CLINE_INCLUDE_DIR}")
endif()

if(NOT DEFINED CLINE_FORMAT OR CLINE_FORMAT STREQUAL "")
    if(CMAKE_HOST_WIN32)
        set(CLINE_FORMAT PE)
    elseif(CMAKE_HOST_APPLE)
        set(CLINE_FORMAT MACHO)
    else()
        set(CLINE_FORMAT ELF)
    endif()
endif()
if(NOT CLINE_FORMAT MATCHES "^(ELF|MACHO|PE)$")
    message(FATAL_ERROR
        "cline.exports: CLINE_FORMAT is \"${CLINE_FORMAT}\". "
        "It must be ELF, MACHO or PE.")
endif()

if(CLINE_FORMAT STREQUAL "PE")
    set(cline_check_tool_name dumpbin)
else()
    set(cline_check_tool_name nm)
endif()
if(NOT DEFINED CLINE_TOOL OR CLINE_TOOL STREQUAL "")
    find_program(cline_check_tool NAMES "${cline_check_tool_name}" NO_CACHE)
    if(NOT cline_check_tool)
        message(FATAL_ERROR
            "cline.exports: ${cline_check_tool_name}, which lists the exported "
            "symbols, was not found on the PATH. Install it, or give its path "
            "with -D CLINE_TOOL=<path>.")
    endif()
    set(CLINE_TOOL "${cline_check_tool}")
elseif(NOT EXISTS "${CLINE_TOOL}" OR IS_DIRECTORY "${CLINE_TOOL}")
    message(FATAL_ERROR
        "cline.exports: ${cline_check_tool_name}, which lists the exported "
        "symbols, was not found: CLINE_TOOL is\n"
        "  ${CLINE_TOOL}\n"
        "which does not exist.")
endif()

# ---------------------------------------------------------------------------
# The expected names, from the public headers.
# ---------------------------------------------------------------------------

file(GLOB_RECURSE cline_check_headers LIST_DIRECTORIES false "${CLINE_INCLUDE_DIR}/*.h")
list(SORT cline_check_headers)
if(NOT cline_check_headers)
    message(FATAL_ERROR
        "cline.exports: no public header (*.h) was found in\n"
        "  ${CLINE_INCLUDE_DIR}")
endif()

set(cline_check_expected "")
set(cline_check_problems "")
foreach(cline_check_header IN LISTS cline_check_headers)
    cline_check_read_header("${cline_check_header}"
        cline_check_expected cline_check_problems)
endforeach()
list(REMOVE_DUPLICATES cline_check_expected)
list(SORT cline_check_expected)

if(NOT cline_check_expected AND cline_check_problems STREQUAL "")
    string(APPEND cline_check_problems
        "  The public headers declare no function with CLINE_API. A public "
        "function is declared as \"CLINE_API <return type> cline_<name>(...);\".\n")
endif()

foreach(cline_check_name IN LISTS cline_check_expected)
    if(NOT cline_check_name MATCHES "^cline_")
        string(APPEND cline_check_problems
            "  ${cline_check_name}: declared with CLINE_API, but the name does "
            "not start with \"cline_\".\n")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# The exported names, and the comparison.
# ---------------------------------------------------------------------------

cline_check_list_exports(cline_check_exported)

foreach(cline_check_name IN LISTS cline_check_exported)
    if(NOT cline_check_name IN_LIST cline_check_expected)
        string(APPEND cline_check_problems
            "  ${cline_check_name}: exported by the library, but not declared "
            "with CLINE_API in a public header.\n")
    endif()
endforeach()
foreach(cline_check_name IN LISTS cline_check_expected)
    if(NOT cline_check_name IN_LIST cline_check_exported)
        string(APPEND cline_check_problems
            "  ${cline_check_name}: declared with CLINE_API in a public "
            "header, but not exported by the library.\n")
    endif()
endforeach()

list(JOIN cline_check_expected " " cline_check_expected_text)
list(JOIN cline_check_exported " " cline_check_exported_text)
if(NOT cline_check_problems STREQUAL "")
    message(FATAL_ERROR
        "cline.exports: the exported symbols do not follow the rules.\n"
        "${cline_check_problems}"
        "Library:        ${CLINE_LIBRARY}\n"
        "Public headers: ${CLINE_INCLUDE_DIR}\n"
        "Declared with CLINE_API: ${cline_check_expected_text}\n"
        "Exported:                 ${cline_check_exported_text}")
endif()

message(STATUS
    "cline.exports: ${CLINE_LIBRARY} exports exactly the functions declared "
    "with CLINE_API: ${cline_check_exported_text}")
