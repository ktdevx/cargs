/*
 * CLine - error codes and their messages.
 */
#include <cline/error.h>

const char *cline_error_message(cline_error_t error)
{
    /*
     * One case for each error code of the list. There is no default label,
     * so that a value outside the list falls through to the end.
     */
    switch (error)
    {
#define CLINE_ERROR_CASE(name, message) \
    case name: \
        return message;
        CLINE_ERROR_LIST(CLINE_ERROR_CASE)
#undef CLINE_ERROR_CASE
    }

    return "unknown error code";
}
