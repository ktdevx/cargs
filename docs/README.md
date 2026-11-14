# ArgX Documentation

This page lists the documents of ArgX, with a one-line description of each. The sections match the directories of `docs/`. The [README](../README.md) at the root of the repository is the starting point for the project.

## Getting started

- [Installation](getting-started/install.md): building and testing ArgX, the build options, the four ways to use it from your project (FetchContent, add_subdirectory, find_package and pkg-config), and the version of an installed package.
- [The example in the README](../README.md#example): a short program to read first.

## Guides

- [Guide](guides/guide.md): the concepts, then a walk through the complete program [examples/pack.c](../examples/pack.c) step by step: value types, the schema, validation, parsing and `--help`, printing the help and parse errors, reading the values and destroying, with recipes.
- [FAQ](guides/faq.md): short answers to common questions about command lines, values, the help and building.
- [Design](guides/design.md): why the library works the way it does, one decision at a time.

## References

Look up a function or a rule here when you need it. The comments of the public headers in `include/argx/` describe each function in detail.

API, one page for each public header, each starting with an index of its functions:

- [Schemas](references/schema.md) (`schema.h`): commands, options, positionals, validation, definition errors, and reading a schema.
- [Value Types](references/types.md) (`type.h`): the built-in, choice and user-defined value types, converting values, and the order of destruction.
- [Parsing](references/parse.md) (`parse.h`): parsing argv, reading the result, parse errors, `--help`, and wide-character argv.
- [Help and Usage](references/help.md) (`help.h`): generating the help and the usage line, the help options, the output, and failures.
- [Allocator](references/alloc.md) (`alloc.h`): setting the allocator, and what its functions must do.
- [Error Codes](references/errors.md) (`error.h`): every error code with its message and meaning.
- [Version](references/version.md) (`version.h`): checking the version in code.

Specifications:

- [Command-Line Syntax](references/syntax.md): the command lines that a program using ArgX accepts.
- [Help Text Layout](references/help-layout.md): the text of the help and of the usage line, and its limits.
- [Ownership and Lifetime](references/ownership.md): who allocates and frees memory, how long things stay valid, and how failures are returned.
- [Versioning](references/versioning.md): how versions are numbered and tagged, and what a new version may change.

## Development

For contributors who change the code, the tests or the documentation, and for maintainers who make releases.

- [Contributing to ArgX](../CONTRIBUTING.md): the entry point for contributors; covers reporting problems, setting up a development environment, building and testing, and submitting a pull request.
- [Development Conventions](development/conventions.md): the rules for naming, the directory layout, writing code and tests, and the version.
- [Development Procedures](development/procedures.md): the steps for adding public functions, headers, sources, tests and error codes, using the test allocator, and verifying a change.
- [Releasing](development/releasing.md): for maintainers; how to release a new version, the repository settings the release process needs, and how to recover when something goes wrong.
- [Changelog](../CHANGELOG.md): the notable changes of each version, for maintainers and for users who upgrade.
