/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * Tests of the public version API: <argx/version.h>.
 */
#include <argx/argx.h>

#include <naos/naos.h>

/* The headers and the linked library are the same version. */
NAOS_TEST(number_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ(argx_version(), ARGX_VERSION_NUMBER);
}

/* Each part taken out of the returned number is the macro of that part. */
NAOS_TEST(major_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ((argx_version() >> 16) & 0xFFUL, ARGX_VERSION_MAJOR);
}

NAOS_TEST(minor_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ((argx_version() >> 8) & 0xFFUL, ARGX_VERSION_MINOR);
}

NAOS_TEST(patch_matches_the_headers)
{
    NAOS_CHECK_UINT_EQ(argx_version() & 0xFFUL, ARGX_VERSION_PATCH);
}

/* The number is 0x00MMmmpp: nothing is set above the major version. */
NAOS_TEST(number_has_no_other_bits)
{
    NAOS_CHECK_UINT_EQ(argx_version() >> 24, 0);
}

/* ARGX_MAKE_VERSION puts each part in its own byte: 0x00MMmmpp. */
NAOS_TEST(make_version_puts_each_part_in_its_byte)
{
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(1, 2, 3), 0x010203UL);
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(1, 0, 0), 0x010000UL);
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(0, 1, 0), 0x000100UL);
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(0, 0, 1), 0x000001UL);
}

/* The bounds: every part 0, every part 255. */
NAOS_TEST(make_version_packs_the_bounds)
{
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(0, 0, 0), 0UL);
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(255, 255, 255), 0xFFFFFFUL);
}

/* One part at 255 does not spill into the bytes of the other parts. */
NAOS_TEST(make_version_keeps_the_bounds_apart)
{
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(255, 0, 0), 0xFF0000UL);
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(0, 255, 0), 0x00FF00UL);
    NAOS_CHECK_UINT_EQ(ARGX_MAKE_VERSION(0, 0, 255), 0x0000FFUL);
}

/*
 * ARGX_VERSION_PRERELEASE is one string literal: it initializes a character
 * array and joins other literals at compile time. The build checks its
 * contents when it reads the version (cmake/ArgXVersion.cmake).
 */
static const char prerelease[] = ARGX_VERSION_PRERELEASE;
static const char joined[] = "[" ARGX_VERSION_PRERELEASE "]";

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
    NAOS_ADD_TEST(make_version_puts_each_part_in_its_byte),
    NAOS_ADD_TEST(make_version_packs_the_bounds),
    NAOS_ADD_TEST(make_version_keeps_the_bounds_apart),
    NAOS_ADD_TEST(prerelease_is_a_string_literal)
)

NAOS_MAIN(
    NAOS_ADD_SUITE(version)
)
