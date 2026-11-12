/*
 * CLArgs - error codes and their messages.
 */
#include <clargs/error.h>

const char *clargs_error_message(clargs_error_t error)
{
    /* One case for each error code of the list. There is no default label,
     * so that a value outside the list falls through to the end. */
    switch (error) {
#define CLARGS_ERROR_CASE(name, message) \
    case name: \
        return message;
        CLARGS_ERROR_LIST(CLARGS_ERROR_CASE)
#undef CLARGS_ERROR_CASE
    }

    return "unknown error code";
}
