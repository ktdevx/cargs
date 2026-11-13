# The library sources. To add a source file, add one line to this list.
set(CLARGS_SOURCES
    alloc.c
    command.c
    error.c
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
add_library(clargs ${CLARGS_SOURCES})

# The same name as the installed target: consumers link to CLArgs::clargs
# whichever way they bring CLArgs in.
add_library(CLArgs::clargs ALIAS clargs)

# Consumers get the public headers only. The internal headers in this
# directory stay private. The include directory of the installed package is
# added by the install rules (cmake/ClargsInstall.cmake).
target_include_directories(clargs
    PUBLIC
        "$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
)

# The ABI version. While the major version is 0, a new minor version may break
# the ABI, so the minor version is part of it.
if(PROJECT_VERSION_MAJOR EQUAL 0)
    set(clargs_soversion "0.${PROJECT_VERSION_MINOR}")
else()
    set(clargs_soversion "${PROJECT_VERSION_MAJOR}")
endif()

set_target_properties(clargs PROPERTIES
    OUTPUT_NAME clargs
    DEBUG_POSTFIX d
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${clargs_soversion}"
)

# C99 without extensions, hidden visibility and the warnings for library code.
# All of it stays on this target.
clargs_target_defaults(clargs STRICT)

# CLARGS_BUILDING: the library itself is being compiled. Never passed on.
# CLARGS_SHARED: the library is a shared library. Passed on to consumers, so
# that they need no definition of their own.
target_compile_definitions(clargs PRIVATE CLARGS_BUILDING)
get_target_property(clargs_type clargs TYPE)
if(clargs_type STREQUAL "SHARED_LIBRARY")
    target_compile_definitions(clargs PUBLIC CLARGS_SHARED)
endif()

# The target for the tests of the internal functions. It exists only when the
# tests are built, and it is never installed. A test that links to it gets
# the public headers and the internal headers of this directory, and includes
# an internal header as "version_internal.h".
if(CLARGS_BUILD_TESTS)
    if(clargs_type STREQUAL "SHARED_LIBRARY")
        # The shared library does not export the internal functions. So the
        # same sources are compiled once more into a static library, as a
        # static clargs would be: with CLARGS_BUILDING and without
        # CLARGS_SHARED. The shared library itself stays as it is.
        add_library(clargs_internal STATIC ${CLARGS_SOURCES})
        target_include_directories(clargs_internal
            PUBLIC
                "${PROJECT_SOURCE_DIR}/include"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
        target_compile_definitions(clargs_internal PRIVATE CLARGS_BUILDING)
        clargs_target_defaults(clargs_internal STRICT)
    else()
        # The static library holds the internal functions already: link to
        # it and add the internal headers. This target compiles no sources,
        # so it takes no compile settings.
        add_library(clargs_internal INTERFACE)
        target_link_libraries(clargs_internal INTERFACE clargs)
        target_include_directories(clargs_internal
            INTERFACE
                "${PROJECT_SOURCE_DIR}/include"
                "${CMAKE_CURRENT_SOURCE_DIR}"
        )
    endif()
endif()
