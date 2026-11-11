/*
 * CLArgs - export marker for public functions.
 *
 * CLARGS_API is the only marker for public functions. Put it at the very
 * beginning of every public function declaration.
 *
 * Two definitions control how it expands:
 *
 *   CLARGS_SHARED    The library is used as a shared library. The build and
 *                    the package information define it for consumers, so
 *                    there is no need to define it by hand.
 *   CLARGS_BUILDING  The library itself is being compiled. Never define it
 *                    in consumer code.
 *
 * With neither defined, the library is used as a static library and
 * CLARGS_API expands to nothing.
 */
#ifndef CLARGS_EXPORT_H
#define CLARGS_EXPORT_H

#if !defined(CLARGS_SHARED)
#define CLARGS_API
#elif defined(_WIN32)
#if defined(CLARGS_BUILDING)
#define CLARGS_API __declspec(dllexport)
#else
#define CLARGS_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define CLARGS_API __attribute__((visibility("default")))
#else
#define CLARGS_API
#endif

#endif /* CLARGS_EXPORT_H */
