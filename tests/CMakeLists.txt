# The tests. Each call registers one test executable with CTest; see
# cmake/ClargsTesting.cmake.
#
# To add a test of the public API, put the file in public/ and add
#     clargs_add_test(<name> SOURCES public/<file>)
# To add a test of the internal functions, put the file in internal/ and add
#     clargs_add_test(<name> SOURCES internal/<file> INTERNAL)

clargs_add_test(version SOURCES public/version_test.c)
clargs_add_test(version_internal SOURCES internal/version_internal_test.c INTERNAL)
