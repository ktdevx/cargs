# Copyright (c) 2026 The ArgX Authors
# SPDX-License-Identifier: MIT
#
# The example "pack", with the command lines that the comment at the top of
# pack.c shows.

argx_add_example(pack)
argx_add_example_test(pack create
    "^verbose: 2, color: on\r?\ncreate out\\.tar, level 6 \\(default\\)\r?\nformat tar, block size 65536 bytes, ratio 0\\.50\r?\n  a\\.txt\r?\n  b\\.txt\r?\n$"
    ARGS -vv create -o out.tar a.txt b.txt)
argx_add_example_test(pack values
    "^verbose: 0, color: off\r?\ncreate out\\.zip, level 9 \\(command line\\)\r?\nformat zip, block size 2097152 bytes, ratio 0\\.50\r?\n  -x\\.txt\r?\n$"
    ARGS --no-color create -l9 --format=zip --block-size 2m -o out.zip -- -x.txt)
argx_add_example_test(pack list
    "^verbose: 0, color: on\r?\nlist out\\.tar, pattern \\* \\(default\\)\r?\n$"
    ARGS list out.tar)
argx_add_example_test(pack help
    "^Usage: pack create \\[OPTIONS\\] --output <FILE> <file>\\.\\.\\.\r?\n\r?\nCreate an archive\\.\r?\n"
    ARGS create --help)
argx_add_example_test(pack root-help
    "^Usage: pack \\[OPTIONS\\] <COMMAND>\r?\n\r?\nPack files into an archive\\.\r?\n"
    ARGS --help)
argx_add_example_test(pack parse-error
    "^pack: missing required option '--output'\r?\nUsage: pack create \\[OPTIONS\\] --output <FILE> <file>\\.\\.\\.\r?\nFor more information, try '--help'\\.\r?\n$"
    ARGS create a.txt)
argx_add_example_test(pack no-command
    "^Usage: pack \\[OPTIONS\\] <COMMAND>\r?\n\r?\nPack files into an archive\\.\r?\n")
# The same command line exits with a failure status.
argx_add_example_failure_test(pack no-command-status)
