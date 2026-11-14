# ArgX Documentation

This page lists the documents of ArgX, grouped by reader, with a one-line description of each; the [README](../README.md) at the root of the repository is the starting point for the project.

Contents:

- [Using ArgX](#using-argx)
- [Contributing to ArgX](#contributing-to-argx)
- [Maintaining ArgX](#maintaining-argx)

## Using ArgX

For users of the library. Read [Schemas](schema.md), [Parsing](parse.md) and [Help and Usage](help.md) in this order; use [Ownership and Lifetime](ownership.md) and [Versioning](versioning.md) as references.

- [Schemas](schema.md): for users who define command lines; covers defining, validating, reading and destroying a schema of commands, options, positionals and value types.
- [Parsing](parse.md): for users who parse argv; covers parsing with a validated schema, the command-line syntax, reading the result, parse errors and destroying the result.
- [Help and Usage](help.md): for users who show help to their own users; covers generating the help and the usage line, the text layout rules, the help options and the output.
- [Ownership and Lifetime](ownership.md): a reference for all users; covers who allocates and frees memory, and how long what you pass and what you get back stay valid.
- [Versioning](versioning.md): a reference for users and contributors; covers how versions are numbered and tagged, what a new version may change, and how to check the version in code.

## Contributing to ArgX

For contributors who change the code, the tests or the documentation.

- [Contributing to ArgX](../CONTRIBUTING.md): the entry point for contributors; covers reporting problems, setting up a development environment, building and testing, and submitting a pull request.
- [Development Conventions](conventions.md): for contributors who change the library; covers the naming conventions, the directory layout, and the procedures and rules for adding public functions, headers, sources and tests.

## Maintaining ArgX

For maintainers who make releases.

- [Releasing](releasing.md): for maintainers; covers how to release a new version, the repository settings the release process needs, and how to recover when something goes wrong.
- [Changelog](../CHANGELOG.md): for maintainers, and for users who upgrade; records the notable changes of each version.
