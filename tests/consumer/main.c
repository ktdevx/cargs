/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * The consumer check: a program that uses ArgX the way a user does.
 *
 * Returns 0 if the version of the linked library equals the version of the
 * headers, and 1 otherwise. When the build passes the version that the
 * package reported (find_package, pkg-config), that version must equal the
 * version of the headers too, the pre-release identifiers included.
 */
#include <stdio.h>
#include <string.h>

#include <argx/argx.h>

int main(void)
{
    unsigned long library = argx_version();
    unsigned long headers = ARGX_VERSION_NUMBER;
    int status = 0;

    if (library != headers)
    {
        printf("argx_version() is 0x%06lx, the headers say 0x%06lx\n", library, headers);
        status = 1;
    }

#ifdef CONSUMER_PACKAGE_VERSION_MAJOR
    if ((CONSUMER_PACKAGE_VERSION_MAJOR != ARGX_VERSION_MAJOR) ||
        (CONSUMER_PACKAGE_VERSION_MINOR != ARGX_VERSION_MINOR) ||
        (CONSUMER_PACKAGE_VERSION_PATCH != ARGX_VERSION_PATCH))
    {
        printf("the package reports version %d.%d.%d, the headers say %d.%d.%d\n",
            CONSUMER_PACKAGE_VERSION_MAJOR,
            CONSUMER_PACKAGE_VERSION_MINOR,
            CONSUMER_PACKAGE_VERSION_PATCH,
            ARGX_VERSION_MAJOR,
            ARGX_VERSION_MINOR,
            ARGX_VERSION_PATCH);
        status = 1;
    }
#endif

#ifdef CONSUMER_PACKAGE_VERSION_PRERELEASE
    if (strcmp(CONSUMER_PACKAGE_VERSION_PRERELEASE, ARGX_VERSION_PRERELEASE) != 0)
    {
        printf("the package reports the pre-release identifiers \"%s\", the headers say \"%s\"\n",
            CONSUMER_PACKAGE_VERSION_PRERELEASE,
            ARGX_VERSION_PRERELEASE);
        status = 1;
    }
#endif

    if (status == 0)
    {
        printf("ArgX %d.%d.%d\n",
            ARGX_VERSION_MAJOR, ARGX_VERSION_MINOR, ARGX_VERSION_PATCH);
    }
    return status;
}
