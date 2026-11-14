/*
 * Tests of the internal version functions: "version_internal.h".
 */
#include <argx/version.h>

#include "version_internal.h"

#include <naos/naos.h>

/* The function packs like the macro of the public header. */
NAOS_TEST(matches_the_macro)
{
    NAOS_CHECK_UINT_EQ(argx_version_pack(1, 2, 3), ARGX_MAKE_VERSION(1, 2, 3));
    NAOS_CHECK_UINT_EQ(argx_version_pack(12, 34, 56), ARGX_MAKE_VERSION(12, 34, 56));
}

/* The layout is 0x00MMmmpp: each part goes to its own byte. */
NAOS_TEST(puts_each_part_in_its_byte)
{
    NAOS_CHECK_UINT_EQ(argx_version_pack(1, 2, 3), 0x010203UL);
    NAOS_CHECK_UINT_EQ(argx_version_pack(1, 0, 0), 0x010000UL);
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 1, 0), 0x000100UL);
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 0, 1), 0x000001UL);
}

/* The lower bound: every part is 0. */
NAOS_TEST(packs_the_lower_bound)
{
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 0, 0), ARGX_MAKE_VERSION(0, 0, 0));
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 0, 0), 0UL);
}

/* The upper bound: every part is 255. */
NAOS_TEST(packs_the_upper_bound)
{
    NAOS_CHECK_UINT_EQ(argx_version_pack(255, 255, 255), ARGX_MAKE_VERSION(255, 255, 255));
    NAOS_CHECK_UINT_EQ(argx_version_pack(255, 255, 255), 0xFFFFFFUL);
}

/* One part at 255 does not spill into the bytes of the other parts. */
NAOS_TEST(keeps_the_bounds_apart)
{
    NAOS_CHECK_UINT_EQ(argx_version_pack(255, 0, 0), ARGX_MAKE_VERSION(255, 0, 0));
    NAOS_CHECK_UINT_EQ(argx_version_pack(255, 0, 0), 0xFF0000UL);
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 255, 0), ARGX_MAKE_VERSION(0, 255, 0));
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 255, 0), 0x00FF00UL);
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 0, 255), ARGX_MAKE_VERSION(0, 0, 255));
    NAOS_CHECK_UINT_EQ(argx_version_pack(0, 0, 255), 0x0000FFUL);
}

/* The version of the headers, packed by the function, is the header number. */
NAOS_TEST(packs_the_header_version)
{
    NAOS_CHECK_UINT_EQ(
        argx_version_pack(ARGX_VERSION_MAJOR, ARGX_VERSION_MINOR, ARGX_VERSION_PATCH),
        ARGX_VERSION_NUMBER);
}

NAOS_SUITE(pack,
    NAOS_ADD_TEST(matches_the_macro),
    NAOS_ADD_TEST(puts_each_part_in_its_byte),
    NAOS_ADD_TEST(packs_the_lower_bound),
    NAOS_ADD_TEST(packs_the_upper_bound),
    NAOS_ADD_TEST(keeps_the_bounds_apart),
    NAOS_ADD_TEST(packs_the_header_version)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(pack)
)
