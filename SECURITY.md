# Security Policy

## Supported Versions

ArgX is in its `0.x` series. Security fixes are made on `main` and shipped in
the next release of the latest minor version. Older minor versions do not
receive fixes; upgrade to the latest release to get them.

| Version                                    | Supported |
|--------------------------------------------|-----------|
| Latest `0.x` release                       | Yes       |
| Older `0.x` releases                       | No        |
| Pre-releases (any, for example `-rc.1`)    | No        |

## Reporting a Vulnerability

**Do not report security vulnerabilities through public issues, pull requests,
or discussions.**

Report them privately through GitHub's private vulnerability reporting:

1. Open the [Security tab](https://github.com/ktdevx/cargs/security) of the
   repository.
2. Select **Report a vulnerability**.
3. Fill in the form.

You can also open the form directly:
<https://github.com/ktdevx/cargs/security/advisories/new>.

Please include as much of the following as you can:

- The ArgX version or commit you tested.
- The platform, compiler, and whether the library was built as static or shared.
- The kind of issue (for example, out-of-bounds read, use after free, integer
  overflow) and the affected function or file.
- Step-by-step instructions to reproduce it, ideally a minimal program and the
  command line that triggers it.
- The impact as you understand it.

## What to Expect

- We aim to acknowledge your report within 7 days.
- We will keep you informed while we confirm the issue and prepare a fix.
- Once a fix is released, we will publish a GitHub security advisory. We will
  credit you in it unless you ask us not to.

Please give us a reasonable amount of time to release a fix before you disclose
the issue publicly.
