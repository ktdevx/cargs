# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- ArgX, a command-line argument parser in plain C99 with no dependencies beyond the C standard library. It builds on Linux (GCC, Clang), macOS (Clang) and Windows (MSVC), as a static or a shared library.
- Schemas of options (flags, counts, `--no-` negatable flags, options with a value), positionals and nested subcommands.
- Typed values: strings, integers, floating-point numbers, choices, and value types you define.
- Parsing of `argv` and of wide-character `argv`, with results that tell what was given and where each value came from, and parse errors with a one-line message.
- Help and usage text generated from the schema, written through your write function or into your buffer.
- No side effects: the library prints nothing, never exits the process and reads no environment variables. The allocator can be replaced.
- Version macros and `argx_version()`, and use from another project through CMake (`FetchContent`, `add_subdirectory`, `find_package`) or pkg-config.

[Unreleased]: https://github.com/ktdevx/cargs/commits/main
