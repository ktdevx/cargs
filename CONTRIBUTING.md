# Contributing to ArgX

Thank you for your interest in contributing to ArgX. Everyone who takes part in this project, including in issues, pull requests and discussions, is expected to follow the [Code of Conduct](CODE_OF_CONDUCT.md).

This document is the entry point for contributors. It covers how to report problems, set up a development environment, build and test, and submit a pull request. It does not repeat the rules for writing code: those are in [Development Conventions](docs/conventions.md). How to use the library is described in the [README](README.md) and the documents it links to.

Contents:

- [Reporting bugs and requesting features](#reporting-bugs-and-requesting-features)
- [Reporting security vulnerabilities](#reporting-security-vulnerabilities)
- [Development environment](#development-environment)
- [Building and testing](#building-and-testing)
- [Coding conventions](#coding-conventions)
- [Documentation and comments](#documentation-and-comments)
- [Submitting a pull request](#submitting-a-pull-request)
- [Recording changes](#recording-changes)
- [Releases](#releases)
- [License](#license)

## Reporting bugs and requesting features

Open an [issue](https://github.com/ktdevx/cargs/issues/new/choose) and choose one of the forms: **Bug report** for a wrong result, a crash or a build failure, and **Feature request** for a new feature or a change. Fill in the fields of the form so that the problem can be reproduced.

Before you start a large change, such as a new public function, a change to existing behavior or a change to the build, open an issue first and discuss it. This avoids work on a change that cannot be accepted in that form.

## Reporting security vulnerabilities

Do not report security vulnerabilities through public issues, pull requests or discussions. Report them privately, following the steps in [SECURITY.md](SECURITY.md).

## Development environment

You need:

- CMake 3.21 or later.
- A C99 compiler.
- Network access when you configure with the tests enabled. The tests are written with the test framework [Naos](https://github.com/vabelorg/naos), and a configuration with tests enabled (the default when you build the repository directly) fetches Naos from GitHub at configure time.

ArgX builds on these environments:

- Linux with GCC and Clang
- macOS with Clang
- Windows with MSVC

If you already have a copy of Naos, you can configure without the network by passing `-DFETCHCONTENT_SOURCE_DIR_NAOS=<path-to-naos>`. The `_deps/naos-src` directory of a build directory that was configured once can be used as the copy. See [Building and testing](README.md#building-and-testing) in the README for this and the other build options.

## Building and testing

From the root of the repository, configure, build and run the tests with these three commands. They are the same commands as in the pull request checklist.

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

Before you submit a change, build and run the tests with both a static and a shared library:

```sh
cmake -S . -B build-static -DCMAKE_BUILD_TYPE=Debug
cmake --build build-static
ctest --test-dir build-static --output-on-failure

cmake -S . -B build-shared -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON
cmake --build build-shared
ctest --test-dir build-shared --output-on-failure
```

These runs include the allocation check and, in the shared configuration, the export check. How to run those checks on their own, and when to also check the Release configuration, is described in [Verifying a change](docs/conventions.md#verifying-a-change).

## Coding conventions

Before you change the code, read [Development Conventions](docs/conventions.md). It describes the naming conventions, where files go, the procedures for adding public functions, headers, sources, tests and error codes, the implementation rules and the notes on writing tests. Follow it instead of choosing names or locations on your own.

The rules for ownership, lifetime, `NULL`, allocators, failures and threads that every public function follows are in [Ownership and Lifetime](docs/ownership.md). How the library's own code applies them is described in [Applying the ownership rules](docs/conventions.md#applying-the-ownership-rules).

## Documentation and comments

Write documentation, code comments and messages in English.

When a change affects behavior or the API, update the documentation in the same pull request: the comments in the public headers, the documents in `docs/`, and the [README](README.md) where it describes the changed behavior.

## Submitting a pull request

1. Fork the repository and create a branch for your change from `main`.
2. Make the change, following [Development Conventions](docs/conventions.md).
3. Add or update tests for the change. The procedures are in [Adding a test of the public API](docs/conventions.md#adding-a-test-of-the-public-api) and [Adding a test of an internal function](docs/conventions.md#adding-a-test-of-an-internal-function). If no test is needed, explain why in the pull request.
4. Build and run the tests locally, as described in [Building and testing](#building-and-testing).
5. Update the documentation and record the change, as described in [Documentation and comments](#documentation-and-comments) and [Recording changes](#recording-changes).
6. Open a pull request against `main`. Fill in the pull request template: describe the change and why it is needed, link the related issues, and go through the checklist.
7. Make sure all CI checks pass. If a check fails, fix the cause and push to the same branch.
8. A maintainer reviews the pull request. Answer the review comments and push the changes to the same branch until the pull request is approved and merged.

## Recording changes

Record every change that is visible to users under `Unreleased` in [CHANGELOG.md](CHANGELOG.md), in the same pull request as the change. Put the entry in the matching category: Added, Changed, Deprecated, Removed, Fixed or Security. The changelog follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/). A release moves the entries under `Unreleased` into the section of the new version, as described in [Keeping the changelog](docs/releasing.md#keeping-the-changelog).

## Releases

This section is for maintainers. The release procedure is described in [docs/releasing.md](docs/releasing.md), and the versioning policy in [docs/versioning.md](docs/versioning.md).

## License

By contributing to ArgX, you agree that your contributions are distributed under the MIT License, as described in [LICENSE](LICENSE).
