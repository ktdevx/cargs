# Version

ArgX follows [Semantic Versioning 2.0.0](https://semver.org/). This document describes how its versions are numbered and what a new version may change, and it is the API reference of `include/argx/version.h`, which tells a program the version of the headers and of the library it uses. The comments of the header describe each macro and function in detail.

Changes in each version are listed in the [changelog](../../CHANGELOG.md).

## Functions

| Functions | Section |
| --- | --- |
| `argx_version` | [Checking the version in code](#checking-the-version-in-code) |

## Version numbers and tags

ArgX follows [Semantic Versioning 2.0.0](https://semver.org/). A version has the form `<major>.<minor>.<patch>`, optionally followed by a hyphen and pre-release identifiers: `0.2.0`, `0.2.0-rc.1`.

In addition to the specification:

- Each of major, minor and patch is a decimal number from 0 to 255, without leading zeros. The range is a limit of the version header (`include/argx/version.h`), which packs the three numbers into one.
- Versions have no build metadata (a `+...` suffix).

Each release is marked by a Git tag of the form `v<version>`, such as `v0.2.0` or `v0.2.0-rc.1`. The GitHub release of a version has the same name as the tag. A published tag is never moved or reused.

## Compatibility

While the major version is 0, ArgX is still in initial development:

- A new minor version (`0.1.x` to `0.2.0`) may contain breaking changes.
- A new patch version (`0.2.0` to `0.2.1`) keeps compatibility. It contains fixes and changes that do not break programs built against the previous patch version of the same minor version.

From 1.0.0 on:

- Only a new major version (`1.x` to `2.0.0`) may contain breaking changes.
- A new minor version adds features in a compatible way.
- A new patch version contains compatible fixes.

The package files and the shared library follow the same boundary:

| | Major version 0 | Major version 1 or later |
| --- | --- | --- |
| Versions that may break compatibility | Minor | Major |
| `find_package(ArgX <version>)` accepts | The same major and minor version, at least the requested one | The same major version, at least the requested one |
| SOVERSION of the shared library | `0.<minor>` | `<major>` |

### What counts as a breaking change

A change is breaking if it does any of the following:

- Removes a public function, type or macro, or changes its meaning. The public API is what the installed headers under `include/argx/` declare.
- Breaks source compatibility: a program that compiled against the previous version no longer compiles, or compiles with a different meaning.
- Breaks the ABI of the shared library: a program linked against the previous version no longer works with the new library without being rebuilt. Changes to the signature of a function, the size or layout of a public type, or the values of public constants and enumerators do this.
- Changes behaviour that is documented, in the comments of the public headers or in the documents under `docs/`.

Such changes are made only in a version that may break compatibility, as described above, and are listed in the changelog.

## Pre-releases

A pre-release, such as `0.2.0-rc.1`, is a test version published before the release it precedes. It lets users try the coming release. A pre-release makes no promise of compatibility, neither with the release that follows it nor with other pre-releases, and it is not supported.

Which versions receive fixes is described in the [security policy](../../SECURITY.md#supported-versions).

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
