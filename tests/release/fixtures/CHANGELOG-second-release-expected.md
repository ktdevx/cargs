# Changelog

All notable changes to this project are documented in this file.

The format follows [Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/),
and the project follows
[Semantic Versioning 2.0.0](https://semver.org/).

## [Unreleased]

## [0.2.0-rc.1] - 2026-12-01

### Changed

- The help text wraps at 80 columns.

### Security

- Option values are checked for length.

## [0.1.0] - 2026-11-13

### Added

- A builder for command-line schemas, with options such as `--name=a;b`.
- Parsing of `argv` into typed values
  (a continuation line of the item above).
  - A nested item, which is not counted as an entry.

### Fixed

* Paths such as `C:\temp\` and brackets such as `[x` are kept as they are.

[Unreleased]: https://github.com/example/project/compare/v0.2.0-rc.1...HEAD
[0.2.0-rc.1]: https://github.com/example/project/compare/v0.1.0...v0.2.0-rc.1
[0.1.0]: https://github.com/example/project/releases/tag/v0.1.0
