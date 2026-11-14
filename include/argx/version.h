/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - library version.
 *
 * This header is the single source of the version. The build reads the four
 * macros below as text, so keep each of them on one line. The three numeric
 * parts are in the form "#define ARGX_VERSION_<PART> <integer>", each in the
 * range 0-255. The pre-release identifiers are in the form
 *
 *     #define ARGX_VERSION_PRERELEASE "<identifiers>"
 *
 * one string literal holding the SemVer 2.0.0 pre-release identifiers without
 * the leading hyphen, such as "rc.1", or an empty string for a release.
 */
#ifndef ARGX_VERSION_H
#define ARGX_VERSION_H

#include <argx/export.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define ARGX_VERSION_MAJOR 0
#define ARGX_VERSION_MINOR 1
#define ARGX_VERSION_PATCH 0

/*
 * The version's pre-release identifiers without the leading hyphen, such
 * as "rc.1". An empty string for a release.
 */
#define ARGX_VERSION_PRERELEASE ""

/* Packs a version into one number: 0x00MMmmpp. Each part is in 0-255. */
#define ARGX_MAKE_VERSION(major, minor, patch) \
    (((unsigned long)(major) << 16) | ((unsigned long)(minor) << 8) | (unsigned long)(patch))

/* The version of these headers, packed with ARGX_MAKE_VERSION. */
#define ARGX_VERSION_NUMBER \
    ARGX_MAKE_VERSION(ARGX_VERSION_MAJOR, ARGX_VERSION_MINOR, ARGX_VERSION_PATCH)

/*
 * Returns the version of the linked library, packed like
 * ARGX_VERSION_NUMBER. Compare the two to check that the headers and the
 * library match:
 *
 *     argx_version() == ARGX_VERSION_NUMBER
 */
ARGX_API unsigned long argx_version(void);

#ifdef __cplusplus
}
#endif

#endif /* ARGX_VERSION_H */
