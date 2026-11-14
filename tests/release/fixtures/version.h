/*
 * A version header in the form of include/argx/version.h, for the tests of
 * the release scripts. It is a release: ARGX_VERSION_PRERELEASE is empty.
 * The macros in this comment do not count:
 *
 * #define ARGX_VERSION_MAJOR 9
 * #define ARGX_VERSION_PRERELEASE "comment.1"
 */
#ifndef ARGX_VERSION_H
#define ARGX_VERSION_H

#define ARGX_VERSION_MAJOR 1
#define ARGX_VERSION_MINOR 2
#define ARGX_VERSION_PATCH 3

/*
 * The pre-release identifiers of the version without the leading hyphen, such as "rc.1".
 * An empty string for a release.
 */
#define ARGX_VERSION_PRERELEASE ""

#endif /* ARGX_VERSION_H */
