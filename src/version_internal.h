/*
 * ArgX - internal declarations for the version.
 *
 * Internal header: it is not installed, and the functions it declares are
 * not exported from the shared library.
 */
#ifndef ARGX_VERSION_INTERNAL_H
#define ARGX_VERSION_INTERNAL_H

/*
 * Packs a version into one number, like ARGX_MAKE_VERSION: 0x00MMmmpp.
 * Each part must be in the range 0-255.
 */
unsigned long argx_version_pack(unsigned int major, unsigned int minor, unsigned int patch);

#endif /* ARGX_VERSION_INTERNAL_H */
