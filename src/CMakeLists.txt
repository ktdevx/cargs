# The library sources. To add a source file, add one line to this list.
set(CLARGS_SOURCES
    version.c
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
