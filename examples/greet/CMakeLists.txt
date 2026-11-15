# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# The example "greet", the program of the README, with the command lines that
# the README shows.

argx_add_example(greet)
argx_add_example_test(greet loud
    "^hello, world!\r?\nhello, world!\r?\ngreeted 2 time\\(s\\)\r?\n$"
    ARGS --loud -n 2 world)
argx_add_example_test(greet missing-name
    "^greet: missing required argument 'name'\r?\nUsage: greet \\[OPTIONS\\] <name>\r?\nFor more information, try '--help'\\.\r?\n$")
argx_add_example_test(greet invalid-value
    "^greet: invalid value 'many' for option '--count' \\(expected integer\\)\r?\nUsage: greet \\[OPTIONS\\] <name>\r?\nFor more information, try '--help'\\.\r?\n$"
    ARGS --count=many world)
argx_add_example_test(greet help
    "^Usage: greet \\[OPTIONS\\] <name>\r?\n\r?\nGreet somebody\\.\r?\n\r?\nArguments:\r?\n  <name>               Whom to greet\\.\r?\n\r?\nOptions:\r?\n  -h, --help           Print this help\\.\r?\n  -l, --loud           Add an exclamation mark\\.\r?\n  -n, --count <COUNT>  How often to greet\\. \\[default: 1\\]\r?\n$"
    ARGS --help)
# The same command lines exit with a failure status.
argx_add_example_failure_test(greet missing-name-status)
argx_add_example_failure_test(greet invalid-value-status ARGS --count=many world)
