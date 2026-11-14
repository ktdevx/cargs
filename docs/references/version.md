# Version

The version of ArgX follows Semantic Versioning, as described in [Versioning](versioning.md). This document is the API reference of `include/argx/version.h`, which describes the version of the headers; `argx_version()` tells the version of the library that is linked.

## Functions

| Functions | Section |
| --- | --- |
| `argx_version` | [Checking the version in code](#checking-the-version-in-code) |

## Checking the version in code

The header `<argx/version.h>` describes the version of the headers:

| Name | Value |
| --- | --- |
| `ARGX_VERSION_MAJOR`, `ARGX_VERSION_MINOR`, `ARGX_VERSION_PATCH` | The numeric parts, as integers |
| `ARGX_VERSION_PRERELEASE` | The pre-release identifiers without the leading hyphen, such as `"rc.1"`, as a string literal; `""` for a release |
| `ARGX_MAKE_VERSION(major, minor, patch)` | Packs a version into one number, `0x00MMmmpp` |
| `ARGX_VERSION_NUMBER` | The version of the headers, packed with `ARGX_MAKE_VERSION` |

`argx_version()` returns the version of the library that is linked, packed like `ARGX_VERSION_NUMBER`. Compare the two to check that the headers and the library match:

```c
if (argx_version() != ARGX_VERSION_NUMBER)
{
    /* The headers and the library are different versions. */
}
```

The packed number holds only the numeric part. It is the same for `0.2.0-rc.1` and `0.2.0`.

`ARGX_VERSION_NUMBER` and `ARGX_MAKE_VERSION()` contain casts, so they cannot be used in `#if`. In `#if`, compare the numeric macros one by one:

```c
#if ARGX_VERSION_MAJOR > 0 || ARGX_VERSION_MINOR >= 2
/* Code for 0.2.0 or later. */
#endif
```

The version of an installed package, for CMake and pkg-config, is in [Installation](../getting-started/install.md#the-version-of-the-package).
