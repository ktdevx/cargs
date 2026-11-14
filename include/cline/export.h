/*
 * CLine - export marker for public functions.
 *
 * CLINE_API is the only marker for public functions. Put it at the very
 * beginning of every public function declaration.
 *
 * Two definitions control how it expands:
 *
 *   CLINE_SHARED    The library is used as a shared library. The build and
 *                    the package information define it for consumers, so
 *                    there is no need to define it by hand.
 *   CLINE_BUILDING  The library itself is being compiled. Never define it
 *                    in consumer code.
 *
 * With neither defined, the library is used as a static library and
 * CLINE_API expands to nothing.
 */
#ifndef CLINE_EXPORT_H
#define CLINE_EXPORT_H

#if !defined(CLINE_SHARED)
#define CLINE_API
#elif defined(_WIN32)
#if defined(CLINE_BUILDING)
#define CLINE_API __declspec(dllexport)
#else
#define CLINE_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define CLINE_API __attribute__((visibility("default")))
#else
#define CLINE_API
#endif

#endif /* CLINE_EXPORT_H */
