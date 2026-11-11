/*
 * Tests of the public version API: <clargs/version.h>.
 */
#include <clargs/clargs.h>

#include <naos/naos.h>

NAOS_TEST(matches_the_headers)
{
    NAOS_CHECK_UINT_EQ(CLARGS_VERSION_NUMBER, clargs_version());
}

NAOS_SUITE(version,
    NAOS_ADD_TEST(matches_the_headers)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(version)
)
