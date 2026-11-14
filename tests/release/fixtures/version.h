/*
 * A version header in the form of include/cline/version.h, for the tests of
 * the release scripts. It is a release: CLINE_VERSION_PRERELEASE is empty.
 * The macros in this comment do not count:
 *
 * #define CLINE_VERSION_MAJOR 9
 * #define CLINE_VERSION_PRERELEASE "comment.1"
 */
#ifndef CLINE_VERSION_H
#define CLINE_VERSION_H

#define CLINE_VERSION_MAJOR 1
#define CLINE_VERSION_MINOR 2
#define CLINE_VERSION_PATCH 3

/*
 * The pre-release identifiers of the version without the leading hyphen, such as "rc.1".
 * An empty string for a release.
 */
#define CLINE_VERSION_PRERELEASE ""

#endif /* CLINE_VERSION_H */
