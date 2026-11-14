/*
 * ArgX - library version.
 */
#include <argx/version.h>

#include "version_internal.h"

unsigned long argx_version_pack(unsigned int major, unsigned int minor, unsigned int patch)
{
    return ((unsigned long)major << 16) | ((unsigned long)minor << 8) | (unsigned long)patch;
}

unsigned long argx_version(void)
{
    return argx_version_pack(ARGX_VERSION_MAJOR, ARGX_VERSION_MINOR, ARGX_VERSION_PATCH);
}
