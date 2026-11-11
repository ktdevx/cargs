/*
 * CLArgs - library version.
 *
 * This header is the single source of the version. The build reads the three
 * macros below as text, so keep each of them on one line in the form
 * "#define CLARGS_VERSION_<PART> <integer>". Each part is in the range 0-255.
 */
#ifndef CLARGS_VERSION_H
#define CLARGS_VERSION_H

#include <clargs/export.h>

#define CLARGS_VERSION_MAJOR 0
#define CLARGS_VERSION_MINOR 1
#define CLARGS_VERSION_PATCH 0

/* Packs a version into one number: 0x00MMmmpp. Each part is in 0-255. */
#define CLARGS_MAKE_VERSION(major, minor, patch) \
    (((unsigned long)(major) << 16) | ((unsigned long)(minor) << 8) | (unsigned long)(patch))

/* The version of these headers, packed with CLARGS_MAKE_VERSION. */
#define CLARGS_VERSION_NUMBER \
    CLARGS_MAKE_VERSION(CLARGS_VERSION_MAJOR, CLARGS_VERSION_MINOR, CLARGS_VERSION_PATCH)

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Returns the version of the linked library, packed like
 * CLARGS_VERSION_NUMBER. Compare the two to check that the headers and the
 * library match:
 *
 *     clargs_version() == CLARGS_VERSION_NUMBER
 */
CLARGS_API unsigned long clargs_version(void);

#ifdef __cplusplus
}
#endif

#endif /* CLARGS_VERSION_H */
