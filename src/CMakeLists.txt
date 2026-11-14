# The library sources. To add a source file, add one line to this list.
set(ARGX_SOURCES
    alloc.c
    command.c
    error.c
    help.c
    help_item.c
    help_options.c
    lex.c
    message.c
    name.c
    number.c
    option.c
    out.c
    parse.c
    positional.c
    resolve.c
    result.c
    str.c
    type.c
    utf8.c
    validate.c
    vec.c
    version.c
    wrap.c
)

# No library type is given, so the standard BUILD_SHARED_LIBS variable chooses
# between a static and a shared library.
add_library(argx ${ARGX_SOURCES})

# The same name as the installed target: consumers link to ArgX::argx
# whichever way they bring ArgX in.
add_library(ArgX::argx ALIAS argx)

# Consumers get the public headers only. The internal headers in this
# directory stay private. The include directory of the installed package is
# added by the install rules (cmake/ArgXInstall.cmake).
target_include_directories(argx
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# The ABI version. While the major version is 0, a new minor version may break
# the ABI, so the minor version is part of it.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(argx_soversion "0.${PROJECT_VERSION_MINOR}")
else()
    set(argx_soversion "${PROJECT_VERSION_MAJOR}")
endif()

set_target_properties(argx PROPERTIES
    OUTPUT_NAME argx
    DEBUG_POSTFIX d
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${argx_soversion}"
)

# C99 without extensions, hidden visibility and the warnings for library code.
# All of it stays on this target.
argx_target_defaults(argx STRICT)

# ARGX_BUILDING: the library itself is being compiled. Never passed on.
# ARGX_SHARED: the library is a shared library. Passed on to consumers, so
# that they need no definition of their own.
target_compile_definitions(argx PRIVATE ARGX_BUILDING)
get_target_property(argx_type argx TYPE)
if(argx_type STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(argx PUBLIC ARGX_SHARED)
endif()

# The target for the tests of the internal functions. It exists only when the
# tests are built, and it is never installed. A test that links to it gets
# the public headers and the internal headers of this directory, and includes
# an internal header as "version_internal.h".
if(ARGX_BUILD_TESTS)
    if(argx_type STREQUAL "SHARED_LIBRARY")
        # The shared library does not export the internal functions. So the
        # same sources are compiled once more into a static library, as a
        # static argx would be: with ARGX_BUILDING and without
        # ARGX_SHARED. The shared library itself stays as it is.
        add_library(argx_internal STATIC ${ARGX_SOURCES})
        target_include_directories(argx_internal
            PUBLIC
                "${PROJECT_SOURCE_DIR}/include"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_compile_definitions(argx_internal PRIVATE ARGX_BUILDING)
        argx_target_defaults(argx_internal STRICT)
    else()
        # The static library holds the internal functions already: link to
        # it and add the internal headers. This target compiles no sources,
        # so it takes no compile settings.
        add_library(argx_internal INTERFACE)
        target_link_libraries(argx_internal INTERFACE argx)
        target_include_directories(argx_internal
            INTERFACE
                "${PROJECT_SOURCE_DIR}/include"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()
