# ArgX Documentation

This page lists the documents of ArgX, grouped by reader, with a one-line description of each; the [README](../README.md) at the root of the repository is the starting point for the project.

## Using ArgX

For users of the library. Start with [Installation](install.md) to build ArgX and use it from your project. Then read the example in the [README](../README.md#example) and the [Guide](guide.md), which goes through every step of the complete program [examples/pack.c](../examples/pack.c), from defining the schema to reading the values. The documents after the Guide are references: look up a function or a rule in them when you need it.

- [Installation](install.md): for users who build ArgX or use it from their project; covers building and testing, the build options, and FetchContent, add_subdirectory, find_package and pkg-config.
- [Guide](guide.md): for users who write a program with ArgX; walks through examples/pack.c step by step: value types, the schema, validation, parsing and `--help`, printing the help and parse errors, reading the values and destroying, with recipes and pitfalls.
- [Schemas](schema.md): a reference for defining command lines; covers defining, validating, reading and destroying a schema of commands, options, positionals and value types.
- [Parsing](parse.md): a reference for parsing argv; covers parsing with a validated schema, the command-line syntax, reading the result, parse errors and destroying the result.
- [Help and Usage](help.md): a reference for showing help to the users of your program; covers generating the help and the usage line, the text layout rules, the help options and the output.
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
