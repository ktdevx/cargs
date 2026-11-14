# Changelog

All notable changes to this project are documented in this file.

The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and the project follows [Semantic Versioning 2.0.0](https://semver.org/).

## [Unreleased]

### Added

- A C99 library for parsing command-line arguments, with no compiler
  extensions and no dependencies beyond the C standard library. It builds on
  Linux (GCC, Clang), macOS (Clang) and Windows (MSVC). Programs include
  `<argx/argx.h>` and link to `ArgX::argx`.
- Schemas built with function calls on opaque handles: a root command
  (`argx_command_create()`), its options, positionals and subcommands,
  descriptions for the help, and validation of the whole schema with
  `argx_command_validate()` before it is used.
- Options of four kinds: flags, counts, negatable flags that are switched
  with `--name` and `--no-name`, and options that take a value. Options can
  have a short name, a long name, a value name, a default value and can be
  required.
- Positional arguments with an arity (required, optional, zero or more, one
  or more) and an optional default value.
- Nested subcommands, each with its own options and positionals.
- Typed values: strings, integers, floating-point numbers and choices from a
  fixed list, and value types defined by the user with their own conversion
  function and an optional cleanup function (`argx_type_create()`).
- Parsing of `argv` with `argx_parse()`, and of wide-character `argv`
  (such as the one of `wmain()`) with `argx_parse_wide()`, which reads
  UTF-16 or UTF-32 and does not depend on the locale.
- Results that tell which subcommands were selected, whether and how often
  options were given, where each value came from (the command line or a
  default) and the converted values.
- Parse errors that report their kind, a one-line message and the argument,
  value, option, positional and command involved. The library prints
  nothing, never exits the process and does not read environment variables.
- Help and usage text generated from the schema, written through a write
  function supplied by the caller (`argx_help_write()`,
  `argx_help_write_usage()`) or into a buffer of the caller
  (`argx_help_format()`, `argx_help_format_usage()`), with settings for
  the width, the indentation and the latest column where descriptions may
  start.
- A replaceable allocator (`argx_set_allocator()`) and error codes with
  messages (`argx_error_t`, `argx_error_message()`).
- Version macros in `<argx/version.h>` (`ARGX_VERSION_MAJOR`,
  `ARGX_VERSION_MINOR`, `ARGX_VERSION_PATCH`,
  `ARGX_VERSION_PRERELEASE`) and `argx_version()` for the version of the
  library that is linked.
- Static and shared library builds (`BUILD_SHARED_LIBS`), and four ways to
  use the library from another project: CMake `FetchContent`,
  `add_subdirectory`, `find_package` and pkg-config (module `argx`).
- Build options `ARGX_BUILD_TESTS`, `ARGX_INSTALL` and
  `ARGX_WARNINGS_AS_ERRORS`, with defaults that add nothing to a parent
  project that embeds the library.

[Unreleased]: https://github.com/ktdevx/cargs/commits/main
