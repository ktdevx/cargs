/*
 * Copyright (c) 2026 The ArgX Authors
 * SPDX-License-Identifier: MIT
 *
 * ArgX - export marker for public functions.
 *
 * ARGX_API is the only marker for public functions. Put it at the very
 * beginning of every public function declaration.
 *
 * Two definitions control how it expands:
 *
 *   ARGX_SHARED    The library is used as a shared library. The build and
 *                    the package information define it for consumers, so
 *                    there is no need to define it by hand.
 *   ARGX_BUILDING  The library itself is being compiled. Never define it
 *                    in consumer code.
 *
 * With neither defined, the library is used as a static library and
 * ARGX_API expands to nothing.
 */
#ifndef ARGX_EXPORT_H
#define ARGX_EXPORT_H

#if !defined(ARGX_SHARED)
#define ARGX_API
#elif defined(_WIN32)
#if defined(ARGX_BUILDING)
#define ARGX_API __declspec(dllexport)
#else
#define ARGX_API __declspec(dllimport)
#endif
#elif defined(__GNUC__)
#define ARGX_API __attribute__((visibility("default")))
#else
#define ARGX_API
#endif

#endif /* ARGX_EXPORT_H */
