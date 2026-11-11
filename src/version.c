/*
 * CLArgs - library version.
 */
#include <clargs/version.h>

#include "version_internal.h"

unsigned long clargs_version_pack(unsigned int major, unsigned int minor, unsigned int patch)
{
    return ((unsigned long)major << 16) | ((unsigned long)minor << 8) | (unsigned long)patch;
}

unsigned long clargs_version(void)
{
    return clargs_version_pack(CLARGS_VERSION_MAJOR, CLARGS_VERSION_MINOR, CLARGS_VERSION_PATCH);
}
