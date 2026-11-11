# ClargsTargetDefaults.cmake - the compile settings shared by the CLArgs targets.
#
#   clargs_target_defaults(<target> [STRICT])
#
# Gives <target>:
#
#   - C99, required, without compiler extensions;
#   - hidden symbol visibility by default, and no export of all symbols on
#     Windows, so that a shared library exports only the functions marked with
#     CLARGS_API, whatever the parent project has set;
#   - warnings:
#
#       compiler     always                    with STRICT
#       GCC, Clang   -Wall -Wextra -Wpedantic  -Wshadow -Wconversion
#                                              -Wstrict-prototypes
#                                              -Wmissing-prototypes
#       MSVC         /W4                       (nothing more)
#
#   - warnings as errors (-Werror, /WX) if the option CLARGS_WARNINGS_AS_ERRORS
#     is ON when the function is called.
#
# The libraries take STRICT. The tests do not: their code expands the macros
# of the test framework.
#
# Every setting stays on <target>. The warnings are PRIVATE and the rest are
# target properties, so nothing is passed on to the targets that link to
# <target>, and no global setting (CMAKE_C_FLAGS, CMAKE_C_STANDARD,
# add_compile_options()) is touched.
#
# <target> must be a target that compiles sources (not an interface library,
# an imported target or an alias). Stops with a FATAL_ERROR otherwise, or if
# an argument other than STRICT is given.

include_guard(GLOBAL)

function(clargs_target_defaults target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "STRICT" "" "")
    if(DEFINED arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "clargs_target_defaults: unknown arguments: ${arg_UNPARSED_ARGUMENTS}. "
            "Usage: clargs_target_defaults(<target> [STRICT]).")
    endif()
    if(NOT TARGET "${target}")
        message(FATAL_ERROR
            "clargs_target_defaults: \"${target}\" is not a target.")
    endif()

    get_target_property(type "${target}" TYPE)
    get_target_property(imported "${target}" IMPORTED)
    get_target_property(aliased "${target}" ALIASED_TARGET)
    if(type STREQUAL "INTERFACE_LIBRARY" OR imported OR aliased)
        message(FATAL_ERROR
            "clargs_target_defaults: \"${target}\" compiles no sources of its own "
            "(interface library, imported target or alias).")
    endif()

    set_target_properties("${target}" PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED ON
        C_EXTENSIONS OFF
        C_VISIBILITY_PRESET hidden
        WINDOWS_EXPORT_ALL_SYMBOLS OFF
    )

    # clang-cl reports the compiler "Clang" but takes the options of MSVC.
    if(CMAKE_C_COMPILER_ID STREQUAL "MSVC"
            OR CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
        set(warnings /W4)
        if(CLARGS_WARNINGS_AS_ERRORS)
            list(APPEND warnings /WX)
        endif()
    elseif(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
        set(warnings -Wall -Wextra -Wpedantic)
        if(arg_STRICT)
            list(APPEND warnings
                -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes)
        endif()
        if(CLARGS_WARNINGS_AS_ERRORS)
            list(APPEND warnings -Werror)
        endif()
    else()
        set(warnings "")
    endif()

    if(warnings)
        target_compile_options("${target}" PRIVATE ${warnings})
    endif()
endfunction()
