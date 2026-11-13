/*
 * A version header in the form of include/clargs/version.h, for the tests of
 * the release scripts. It is a release: CLARGS_VERSION_PRERELEASE is empty.
 * The macros in this comment do not count:
 *
 * #define CLARGS_VERSION_MAJOR 9
 * #define CLARGS_VERSION_PRERELEASE "comment.1"
 */
#ifndef CLARGS_VERSION_H
#define CLARGS_VERSION_H

#define CLARGS_VERSION_MAJOR 1
#define CLARGS_VERSION_MINOR 2
#define CLARGS_VERSION_PATCH 3

/* The pre-release identifiers of the version without the leading hyphen, such as "rc.1".
   An empty string for a release. */
#define CLARGS_VERSION_PRERELEASE ""

#endif /* CLARGS_VERSION_H */
