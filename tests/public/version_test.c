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

NAOS_SUITE(version,
    NAOS_ADD_TEST(number_matches_the_headers),
    NAOS_ADD_TEST(major_matches_the_headers),
    NAOS_ADD_TEST(minor_matches_the_headers),
    NAOS_ADD_TEST(patch_matches_the_headers),
    NAOS_ADD_TEST(number_has_no_other_bits)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(version)
)
