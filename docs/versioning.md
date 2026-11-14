# Versioning

This document describes how CLine numbers its versions, what a new version
may change, and how a program can check the version it uses. It is meant for
users of the library and for contributors.

Changes in each version are listed in the [changelog](../CHANGELOG.md). How a
version is released is described in [the release process](releasing.md).

## Version numbers and tags

CLine follows [Semantic Versioning 2.0.0](https://semver.org/). A version has
the form `<major>.<minor>.<patch>`, optionally followed by a hyphen and
pre-release identifiers: `0.2.0`, `0.2.0-rc.1`.

In addition to the specification:

- Each of major, minor and patch is a decimal number from 0 to 255, without
  leading zeros. The range is a limit of the version header
  (`include/cline/version.h`), which packs the three numbers into one.
- Versions have no build metadata (a `+...` suffix).

Each release is marked by a Git tag of the form `v<version>`, such as
`v0.2.0` or `v0.2.0-rc.1`. The GitHub release of a version has the same name
as the tag. A published tag is never moved or reused.

The version is kept in one place, `include/cline/version.h`. It is not
changed by hand: the release process updates it together with the changelog.

## Compatibility before and after 1.0

While the major version is 0, CLine is still in initial development:

- A new minor version (`0.1.x` to `0.2.0`) may contain breaking changes.
- A new patch version (`0.2.0` to `0.2.1`) keeps compatibility. It contains
  fixes and changes that do not break programs built against the previous
  patch version of the same minor version.

From 1.0.0 on:

- Only a new major version (`1.x` to `2.0.0`) may contain breaking changes.
- A new minor version adds features in a compatible way.
- A new patch version contains compatible fixes.

The package files and the shared library follow the same boundary:

| | Major version 0 | Major version 1 or later |
|---|---|---|
| Versions that may break compatibility | Minor | Major |
| `find_package(CLine <version>)` accepts | The same major and minor version, at least the requested one | The same major version, at least the requested one |
| SOVERSION of the shared library | `0.<minor>` | `<major>` |

So while the major version is 0, ask `find_package` for a minor version, such
as `find_package(CLine 0.2)`. `find_package(CLine 0)` and version ranges
that span more than one minor version, such as `0.1...1`, do not find
version 0.1 or later.

## What counts as a breaking change

A change is breaking if it does any of the following:

- Removes a public function, type or macro, or changes its meaning. The public
  API is what the installed headers under `include/cline/` declare.
- Breaks source compatibility: a program that compiled against the previous
  version no longer compiles, or compiles with a different meaning.
- Breaks the ABI of the shared library: a program linked against the previous
  version no longer works with the new library without being rebuilt. Changes
  to the signature of a function, the size or layout of a public type, or the
  values of public constants and enumerators do this.
- Changes behaviour that is documented, in the comments of the public headers
  or in the documents under `docs/`.

Such changes are made only in a version that may break compatibility, as
described above, and are listed in the changelog.

## Pre-releases

A pre-release, such as `0.2.0-rc.1`, is a test version published before the
release it precedes. It lets users try the coming release. A pre-release makes
no promise of compatibility, neither with the release that follows it nor with
other pre-releases, and it is not supported (see
[Supported versions](#supported-versions)).

Under Semantic Versioning 2.0.0, a pre-release has a lower precedence than its
release: `0.2.0-rc.1` comes before `0.2.0`. Two build tools compare versions in
a different way:

- `find_package` compares only the numeric part, `<major>.<minor>.<patch>`. So
  `find_package(CLine 0.2)` and `find_package(CLine 0.2.0)` also find an
  installed `0.2.0-rc.1`. To tell a pre-release from a release, check
  `CLine_VERSION_PRERELEASE` after `find_package`; it is empty for a release.
- pkg-config compares the full version, and considers `0.2.0-rc.1` newer than
  `0.2.0`. So `pkg-config --atleast-version=0.2.0 cline` also accepts an
  installed `0.2.0-rc.1`.

## Supported versions

Which versions receive fixes is described in the
[security policy](../SECURITY.md#supported-versions).

## Checking the version in code

The header `<cline/version.h>` describes the version of the headers:

| Name | Value |
|---|---|
| `CLINE_VERSION_MAJOR`, `CLINE_VERSION_MINOR`, `CLINE_VERSION_PATCH` | The numeric parts, as integers |
| `CLINE_VERSION_PRERELEASE` | The pre-release identifiers without the leading hyphen, such as `"rc.1"`, as a string literal; `""` for a release |
| `CLINE_MAKE_VERSION(major, minor, patch)` | Packs a version into one number, `0x00MMmmpp` |
| `CLINE_VERSION_NUMBER` | The version of the headers, packed with `CLINE_MAKE_VERSION` |

`cline_version()` returns the version of the library that is linked, packed
like `CLINE_VERSION_NUMBER`. Compare the two to check that the headers and the
library match:

```c
if (cline_version() != CLINE_VERSION_NUMBER)
{
    /* The headers and the library are different versions. */
}
```

The packed number holds only the numeric part. It is the same for
`0.2.0-rc.1` and `0.2.0`.

`CLINE_VERSION_NUMBER` and `CLINE_MAKE_VERSION()` contain casts, so they
cannot be used in `#if`. In `#if`, compare the numeric macros one by one:

```c
#if CLINE_VERSION_MAJOR > 0 || CLINE_VERSION_MINOR >= 2
/* Code for 0.2.0 or later. */
#endif
```

The installed package files hold the version as well:

| Where | Value |
|---|---|
| `CLine_VERSION`, set by `find_package(CLine)` | The numeric part, such as `0.2.0` |
| `CLine_VERSION_FULL`, set by `find_package(CLine)` | The full version, such as `0.2.0-rc.1`; equal to `CLine_VERSION` for a release |
| `CLine_VERSION_PRERELEASE`, set by `find_package(CLine)` | The pre-release identifiers, such as `rc.1`; empty for a release |
| `Version` in `cline.pc` (`pkg-config --modversion cline`) | The full version, such as `0.2.0-rc.1` |

These variables are set only by `find_package`. A project that adds CLine
with `FetchContent` or `add_subdirectory` reads the version from the header.
