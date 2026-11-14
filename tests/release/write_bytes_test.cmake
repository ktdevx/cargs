# Tests of cline_write_bytes() and cline_check_write_bytes() in
# tools/release/ClineReadBytes.cmake: a text is written byte for byte, line
# endings included, both with file(WRITE), as on Linux and macOS, and with
# file(CONFIGURE), as on Windows (forced here with
# CLINE_WRITE_BYTES_WITH_CONFIGURE). A text that cannot be written exactly
# stops the write with the reason.
#
# Run by CTest as release.write_bytes; see ReleaseTestHelpers.cmake.

cmake_minimum_required(VERSION 3.21)

include("${CMAKE_CURRENT_LIST_DIR}/ReleaseTestHelpers.cmake")

# Texts that every way of writing keeps byte for byte.
set(names lf crlf mixed lone_cr empty escapes cmakedefine blank_lines semicolons)
set(text_lf "first\nsecond\n")
set(text_crlf "first\r\nsecond\r\n")
set(text_mixed "first\r\nsecond\nthird\r\n")
set(text_lone_cr "a\rb\n\r\r\n\r\n")
set(text_empty "")
string(CONCAT text_escapes
    "@VERSION@ a@b.c @ @@ @cline_at@\n"
    "\${CMAKE_COMMAND} \${x} \$ENV{PATH} \$\n"
    "C:\\temp\\ \\n \\@x \"quoted\"\n")
string(CONCAT text_cmakedefine
    "#cmakedefine FOO\n"
    "#  cmakedefine01 BAR\r\n"
    "# cmakedefine BAZ @BAZ@\n"
    "cmakedefine\n")
set(text_blank_lines "\n\n\r\n\nend\n\n")
set(text_semicolons "a;b;[c]\n;\n")

# Writes each text with the current way of writing and checks the bytes of the
# file. <mode> names the way in the descriptions and the file names.
function(expect_written mode)
    foreach(name IN LISTS names)
        set(path "${CLINE_TEST_WORK_DIR}/${mode}/dir-${name}/${name}.txt")
        cline_write_bytes("${path}" "${text_${name}}" "the text ${name}")
        file(READ "${path}" actual HEX)
        string(HEX "${text_${name}}" expected)
        cline_test_expect_equal("${actual}" "${expected}" "${mode}, ${name}: the bytes of the file")
    endforeach()

    # An existing file is replaced, by a longer and by a shorter text.
    set(path "${CLINE_TEST_WORK_DIR}/${mode}/replaced.txt")
    cline_write_bytes("${path}" "${text_lf}" "the text")
    cline_write_bytes("${path}" "${text_mixed}" "the text")
    file(READ "${path}" actual HEX)
    string(HEX "${text_mixed}" expected)
    cline_test_expect_equal("${actual}" "${expected}" "${mode}, replaced by a longer text")
    cline_write_bytes("${path}" "x\n" "the text")
    file(READ "${path}" actual HEX)
    cline_test_expect_equal("${actual}" "780a" "${mode}, replaced by a shorter text")
endfunction()

# --- Writing -----------------------------------------------------------------

# Natively: file(WRITE), or file(CONFIGURE) on Windows.
set(CLINE_WRITE_BYTES_WITH_CONFIGURE OFF)
expect_written("native")

set(CLINE_WRITE_BYTES_WITH_CONFIGURE ON)
expect_written("configure")

# A path with "..".
cline_write_bytes("${CLINE_TEST_WORK_DIR}/configure/../dotdot.txt" "${text_crlf}" "the text")
file(READ "${CLINE_TEST_WORK_DIR}/dotdot.txt" actual HEX)
string(HEX "${text_crlf}" expected)
cline_test_expect_equal("${actual}" "${expected}" "configure, a path with ..")

# --- Failures ----------------------------------------------------------------
#
# Each case runs in a child process, since a FATAL_ERROR would stop this
# script. The text is passed in a file, read byte for byte.

set(driver "${CLINE_TEST_WORK_DIR}/driver.cmake")
file(WRITE "${driver}" "
include(\"${CLINE_SOURCE_ROOT}/tools/release/ClineReadBytes.cmake\")
cline_read_bytes(\"\${TEXT_FILE}\" \"the input\" text)
if(ACTION STREQUAL \"check\")
    cline_check_write_bytes(\"\${OUT}\" \"\${text}\" \"the output\")
else()
    cline_write_bytes(\"\${OUT}\" \"\${text}\" \"the output\")
endif()
message(\"done: [ok]\")
")

set(case_number 0)

# Runs the driver on <text> with <action> (write or check) and the -D
# arguments that follow. Sets <prefix>_RESULT, <prefix>_OUTPUT (with blanks and
# line endings made one blank) and <prefix>_OUT (the path of the output file).
function(run_driver prefix action text)
    math(EXPR number "${case_number} + 1")
    set(case_number ${number} PARENT_SCOPE)
    set(text_file "${CLINE_TEST_WORK_DIR}/case-${number}.in")
    set(out "${CLINE_TEST_WORK_DIR}/case-${number}.out")
    # The input is written natively, which keeps every byte on Linux and macOS;
    # on Windows, only texts without LF are given here.
    file(WRITE "${text_file}" "${text}")
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -D "TEXT_FILE=${text_file}" -D "OUT=${out}"
            -D "ACTION=${action}" ${ARGN} -P "${driver}"
        WORKING_DIRECTORY "${CLINE_TEST_WORK_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
    )
    string(REGEX REPLACE "[ \t\r\n]+" " " flat "${output}${error}")
    set(${prefix}_RESULT "${result}" PARENT_SCOPE)
    set(${prefix}_OUTPUT "${flat}" PARENT_SCOPE)
    set(${prefix}_OUT "${out}" PARENT_SCOPE)
endfunction()

# A text that is not empty and does not end with LF cannot be written with
# file(CONFIGURE): the write stops with the reason and no file is written.
foreach(text IN ITEMS "no line ending" "a\rb" "a\r")
    string(HEX "${text}" id)
    foreach(action IN ITEMS write check)
        run_driver(run ${action} "${text}" -D CLINE_WRITE_BYTES_WITH_CONFIGURE=ON)
        set(what "configure, ${action} ${id}")
        if(run_RESULT EQUAL 0)
            cline_test_expect_equal("${run_RESULT}" "non-zero" "${what}: the outcome")
        else()
            cline_test_expect_equal("failed" "failed" "${what}: the outcome")
        endif()
        cline_test_expect_not_contains("${run_OUTPUT}" "done: [ok]" "${what}: no result")
        cline_test_expect_contains("${run_OUTPUT}" "error: cannot write the output ${run_OUT}"
            "${what}: the error names the file")
        cline_test_expect_contains("${run_OUTPUT}" "ends with a line ending" "${what}: the reason")
        if(EXISTS "${run_OUT}")
            set(exists "a file")
        else()
            set(exists "no file")
        endif()
        cline_test_expect_equal("${exists}" "no file" "${what}: no file is written")
    endforeach()
endforeach()

# The same text is written exactly without file(CONFIGURE), except on Windows.
if(NOT CMAKE_HOST_WIN32)
    run_driver(run write "no line ending")
    cline_test_expect_equal("${run_RESULT}" "0" "write, no line ending: the outcome")
    cline_test_expect_contains("${run_OUTPUT}" "done: [ok]" "write, no line ending: the result")
    file(READ "${run_OUT}" actual HEX)
    string(HEX "no line ending" expected)
    cline_test_expect_equal("${actual}" "${expected}" "write, no line ending: the bytes of the file")
endif()

# The driver itself works, so the failures above come from the cases: an empty
# text, which has no line ending either, is written.
foreach(action IN ITEMS check write)
    run_driver(run ${action} "" -D CLINE_WRITE_BYTES_WITH_CONFIGURE=ON)
    cline_test_expect_equal("${run_RESULT}" "0" "driver, ${action} an empty text: the exit code")
    cline_test_expect_contains("${run_OUTPUT}" "done: [ok]" "driver, ${action} an empty text: the result")
endforeach()
file(READ "${run_OUT}" actual HEX)
cline_test_expect_equal("${actual}" "" "driver, write an empty text: the file is empty")

cline_test_finish()
