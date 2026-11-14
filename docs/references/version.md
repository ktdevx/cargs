# Version

How ArgX versions are numbered and what they may change, and the API reference of `include/argx/version.h`. Changes are listed in the [changelog](../../CHANGELOG.md).

## Functions

| Functions | Section |
| --- | --- |
| `argx_version` | [Checking the version in code](#checking-the-version-in-code) |

## Version numbers and tags

ArgX follows [Semantic Versioning 2.0.0](https://semver.org/): `0.2.0`, `0.2.0-rc.1`. In addition:

- Major, minor and patch are decimal 0 to 255 without leading zeros, since the header packs them into one number.
- There is no build metadata (`+...`).
- Each release is tagged `v<version>`. A published tag is never moved or reused.

## Compatibility

| | Major version 0 | Major version 1 or later |
| --- | --- | --- |
| May break compatibility | Minor (`0.1.x` to `0.2.0`) | Major (`1.x` to `2.0.0`) |
| Compatible additions | — | Minor |
| Compatible fixes | Patch | Patch |
| `find_package(ArgX <version>)` accepts | The same major and minor, at least the requested one | The same major, at least the requested one |
| SOVERSION | `0.<minor>` | `<major>` |

### What counts as a breaking change

- Removing a public function, type or macro declared under `include/argx/`, or changing its meaning.
- A program that compiled against the previous version no longer compiles, or compiles with a different meaning.
- A program linked against the previous shared library no longer works without a rebuild: a changed signature, a changed size or layout of a public type, or changed values of public constants.
- Changing behaviour documented in the public headers or in `docs/`.

## Pre-releases

A pre-release such as `0.2.0-rc.1` lets users try a coming release. It promises no compatibility and is not supported. Supported versions are in the [security policy](../../SECURITY.md#supported-versions).

## Checking the version in code

| Name | Value |
| --- | --- |
| `ARGX_VERSION_MAJOR`, `ARGX_VERSION_MINOR`, `ARGX_VERSION_PATCH` | The numeric parts |
| `ARGX_VERSION_PRERELEASE` | The pre-release identifiers as a string literal, such as `"rc.1"`; `""` for a release |
| `ARGX_MAKE_VERSION(major, minor, patch)` | Packs a version into `0x00MMmmpp` |
| `ARGX_VERSION_NUMBER` | The version of the headers, packed |

`argx_version()` returns the packed version of the linked library. The packed number ignores the pre-release part.

```c
if (argx_version() != ARGX_VERSION_NUMBER)
{
    /* The headers and the library are different versions. */
}
```

The packed macros contain casts and cannot be used in `#if`; compare the numeric macros there:

```c
#if (ARGX_VERSION_MAJOR > 0) || (ARGX_VERSION_MINOR >= 2)
/* Code for 0.2.0 or later. */
#endif
```

The version of an installed package is in [Installation](../getting-started/installation.md#the-version-of-the-package).
