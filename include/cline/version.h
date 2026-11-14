/*
 * CLine - library version.
 *
 * This header is the single source of the version. The build reads the four
 * macros below as text, so keep each of them on one line. The three numeric
 * parts are in the form "#define CLINE_VERSION_<PART> <integer>", each in the
 * range 0-255. The pre-release identifiers are in the form
 *
 *     #define CLINE_VERSION_PRERELEASE "<identifiers>"
 *
 * one string literal holding the SemVer 2.0.0 pre-release identifiers without
 * the leading hyphen, such as "rc.1", or an empty string for a release.
 */
#ifndef CLINE_VERSION_H
#define CLINE_VERSION_H

#include <cline/export.h>

#define CLINE_VERSION_MAJOR 0
#define CLINE_VERSION_MINOR 1
#define CLINE_VERSION_PATCH 0
/*
 * The pre-release identifiers of the version without the leading hyphen, such as "rc.1".
 * An empty string for a release.
 */
#define CLINE_VERSION_PRERELEASE ""

/* Packs a version into one number: 0x00MMmmpp. Each part is in 0-255. */
#define CLINE_MAKE_VERSION(major, minor, patch) \
    (((unsigned long)(major) << 16) | ((unsigned long)(minor) << 8) | (unsigned long)(patch))

/* The version of these headers, packed with CLINE_MAKE_VERSION. */
#define CLINE_VERSION_NUMBER \
    CLINE_MAKE_VERSION(CLINE_VERSION_MAJOR, CLINE_VERSION_MINOR, CLINE_VERSION_PATCH)

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Returns the version of the linked library, packed like
 * CLINE_VERSION_NUMBER. Compare the two to check that the headers and the
 * library match:
 *
 *     cline_version() == CLINE_VERSION_NUMBER
 */
CLINE_API unsigned long cline_version(void);

#ifdef __cplusplus
}
#endif

#endif /* CLINE_VERSION_H */
