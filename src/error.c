/*
 * ArgX - error codes and their messages.
 */
#include <argx/error.h>

const char *argx_error_message(argx_error_t error)
{
    /*
     * One case for each error code of the list. There is no default label,
     * so that a value outside the list falls through to the end.
     */
    switch (error)
    {
#define ARGX_ERROR_CASE(name, message) \
    case name: \
        return message;
        ARGX_ERROR_LIST(ARGX_ERROR_CASE)
#undef ARGX_ERROR_CASE
    }

    return "unknown error code";
}
