/*
 * Tests of the public version API: <clargs/version.h>.
 */
#include <clargs/clargs.h>

#include <naos/naos.h>

/* The headers and the linked library are the same version. */
NAOS_TEST(number_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ(clargs_version(), CLARGS_VERSION_NUMBER);
}

/* Each part taken out of the returned number is the macro of that part. */
NAOS_TEST(major_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ((clargs_version() >> 16) & 0xFFUL, CLARGS_VERSION_MAJOR);
}

NAOS_TEST(minor_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ((clargs_version() >> 8) & 0xFFUL, CLARGS_VERSION_MINOR);
}

NAOS_TEST(patch_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ(clargs_version() & 0xFFUL, CLARGS_VERSION_PATCH);
}

/* The number is 0x00MMmmpp: nothing is set above the major version. */
NAOS_TEST(number_has_no_other_bits)
{
    NAOS_CHECK_UINT_EQ(clargs_version() >> 24, 0);
}

/*
 * CLARGS_VERSION_PRERELEASE is one string literal: it initializes a character
 * array and joins other literals at compile time. The build checks its
 * contents when it reads the version (cmake/ClargsVersion.cmake).
 */
static const char prerelease[] = CLARGS_VERSION_PRERELEASE;
static const char joined[] = "[" CLARGS_VERSION_PRERELEASE "]";

NAOS_TEST(prerelease_is_a_string_literal)
{
    NAOS_CHECK_UINT_EQ(sizeof joined, sizeof prerelease + 2);
    NAOS_CHECK_INT_EQ(joined[0], '[');
    NAOS_CHECK_INT_EQ(joined[sizeof joined - 2], ']');
    NAOS_CHECK_MEM_EQ(joined + 1, sizeof prerelease - 1, prerelease, sizeof prerelease - 1);
}

NAOS_SUITE(version,
    NAOS_ADD_TEST(number_matches_the_headers),
    NAOS_ADD_TEST(major_matches_the_headers),
    NAOS_ADD_TEST(minor_matches_the_headers),
    NAOS_ADD_TEST(patch_matches_the_headers),
    NAOS_ADD_TEST(number_has_no_other_bits),
    NAOS_ADD_TEST(prerelease_is_a_string_literal)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(version)
)
