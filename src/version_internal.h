/*
 * CLArgs - internal declarations for the version.
 *
 * Internal header: it is not installed, and the functions it declares are
 * not exported from the shared library.
 */
#ifndef CLARGS_VERSION_INTERNAL_H
#define CLARGS_VERSION_INTERNAL_H

/*
 * Packs a version into one number, like CLARGS_MAKE_VERSION: 0x00MMmmpp.
 * Each part must be in the range 0-255.
 */
unsigned long clargs_version_pack(unsigned int major, unsigned int minor, unsigned int patch);

#endif /* CLARGS_VERSION_INTERNAL_H */
