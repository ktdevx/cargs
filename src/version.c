/*
 * CLine - library version.
 */
#include <cline/version.h>

#include "version_internal.h"

unsigned long cline_version_pack(unsigned int major, unsigned int minor, unsigned int patch)
{
    return ((unsigned long)major << 16) | ((unsigned long)minor << 8) | (unsigned long)patch;
}

unsigned long cline_version(void)
{
    return cline_version_pack(CLINE_VERSION_MAJOR, CLINE_VERSION_MINOR, CLINE_VERSION_PATCH);
}
