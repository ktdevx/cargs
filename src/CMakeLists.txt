# The library sources. To add a source file, add one line to this list.
set(CLINE_SOURCES
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
add_library(cline ${CLINE_SOURCES})

# The same name as the installed target: consumers link to CLine::cline
# whichever way they bring CLine in.
add_library(CLine::cline ALIAS cline)

# Consumers get the public headers only. The internal headers in this
# directory stay private. The include directory of the installed package is
# added by the install rules (cmake/ClineInstall.cmake).
target_include_directories(cline
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# The ABI version. While the major version is 0, a new minor version may break
# the ABI, so the minor version is part of it.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(cline_soversion "0.${PROJECT_VERSION_MINOR}")
else()
    set(cline_soversion "${PROJECT_VERSION_MAJOR}")
endif()

set_target_properties(cline PROPERTIES
    OUTPUT_NAME cline
    DEBUG_POSTFIX d
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${cline_soversion}"
)

# C99 without extensions, hidden visibility and the warnings for library code.
# All of it stays on this target.
cline_target_defaults(cline STRICT)

# CLINE_BUILDING: the library itself is being compiled. Never passed on.
# CLINE_SHARED: the library is a shared library. Passed on to consumers, so
# that they need no definition of their own.
target_compile_definitions(cline PRIVATE CLINE_BUILDING)
get_target_property(cline_type cline TYPE)
if(cline_type STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(cline PUBLIC CLINE_SHARED)
endif()

# The target for the tests of the internal functions. It exists only when the
# tests are built, and it is never installed. A test that links to it gets
# the public headers and the internal headers of this directory, and includes
# an internal header as "version_internal.h".
if(CLINE_BUILD_TESTS)
    if(cline_type STREQUAL "SHARED_LIBRARY")
        # The shared library does not export the internal functions. So the
        # same sources are compiled once more into a static library, as a
        # static cline would be: with CLINE_BUILDING and without
        # CLINE_SHARED. The shared library itself stays as it is.
        add_library(cline_internal STATIC ${CLINE_SOURCES})
        target_include_directories(cline_internal
            PUBLIC
                "${PROJECT_SOURCE_DIR}/include"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_compile_definitions(cline_internal PRIVATE CLINE_BUILDING)
        cline_target_defaults(cline_internal STRICT)
    else()
        # The static library holds the internal functions already: link to
        # it and add the internal headers. This target compiles no sources,
        # so it takes no compile settings.
        add_library(cline_internal INTERFACE)
        target_link_libraries(cline_internal INTERFACE cline)
        target_include_directories(cline_internal
            INTERFACE
                "${PROJECT_SOURCE_DIR}/include"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()
