# Runs one example and compares what it printed with the output it is known
# to give. The ctest counterpart of `make examples`, for toolchains without
# diff, which on Windows is most of them.
#
#   cmake -DPROGRAM=... -DEXPECTED=... -DEXIT_CODE=0 [-DARGS=a|b|c] -P check_example.cmake
#
# Line endings are ignored: a Windows program writes CRLF to stdout, and the
# expected files are checked out with whatever git chose.

# Arguments arrive joined with "|": a list with semicolons does not survive
# the trip through add_test and -D intact.
string(REPLACE "|" ";" ARGS "${ARGS}")

execute_process(
  COMMAND "${PROGRAM}" ${ARGS}
  OUTPUT_VARIABLE actual
  RESULT_VARIABLE status)

if(NOT status EQUAL EXIT_CODE)
  message(FATAL_ERROR "${PROGRAM} exited with ${status}, expected ${EXIT_CODE}")
endif()

file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" actual "${actual}")
string(REPLACE "\r\n" "\n" expected "${expected}")

if(NOT actual STREQUAL expected)
  message(FATAL_ERROR "Output differs from ${EXPECTED}\n--- got ---\n${actual}")
endif()
