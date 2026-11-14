/*
 * CLArgs - internal declarations for the help: the contents of the help
 * options, their default values and their limit.
 *
 * Internal header: it is not installed. Only help_options.c writes the
 * fields of the help options; the generation of the help only reads them.
 */
#ifndef CLARGS_HELP_INTERNAL_H
#define CLARGS_HELP_INTERNAL_H

#include <clargs/help.h>

/*
 * The default values of the settings, in columns. They are also what the
 * generation uses when it is given no help options.
 */
#define CLARGS_HELP_DEFAULT_WIDTH ((size_t)80)
#define CLARGS_HELP_DEFAULT_INDENT ((size_t)2)
#define CLARGS_HELP_DEFAULT_MAX_COLUMN ((size_t)32)

/*
 * The greatest value of a setting. A greater value is kept as this one, so
 * sums of a few settings and widths never overflow a size_t, which holds
 * at least 65535.
 */
#define CLARGS_HELP_SETTING_MAX ((size_t)65535)

/*
 * The help options. Every field is at most CLARGS_HELP_SETTING_MAX.
 */
struct clargs_help_options
{
    size_t width;      /* The width in columns; 0 means no limit. */
    size_t indent;     /* The blanks before each item of a list. */
    size_t max_column; /* The last column at which the descriptions may start. */
};

#endif /* CLARGS_HELP_INTERNAL_H */
