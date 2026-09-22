# ci18n build rules
#
#   make           build the example and build-and-run the tests
#   make example   build the example only
#   make test      build and run the unit tests
#   make clean     remove build artifacts
#
# Override the compiler or add flags without losing the defaults:
#   make CC=clang
#   make EXTRA_CFLAGS=-Werror

# Default to gcc, but honour CC from the environment or the command line.
# A plain `CC ?= gcc` would not work here: make predefines CC as `cc`.
ifeq ($(origin CC),default)
CC = gcc
endif

# The language standard is separate so CI can sweep it without restating the
# warning flags: make CSTD=c11
CSTD ?= c99

CFLAGS ?= -Wall -Wextra -std=$(CSTD)
CPPFLAGS += -I./include

ALL_CFLAGS = $(CFLAGS) $(EXTRA_CFLAGS) $(CPPFLAGS)

.PHONY: all clean example test test-threads fuzz fuzz-run fuzz-replay fuzz-corpus

all: example test

example: examples/example.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) -o example examples/example.c

# Builds and runs the suite. The binary exits non-zero if any test fails.
test: tests/test_ci18n.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) -o test_ci18n tests/test_ci18n.c
	./test_ci18n

# Thread-local context tests. Kept out of `all` because they need pthreads,
# which not every host running `make` will have.
test-threads: tests/test_thread_local.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) -pthread -o test_thread_local tests/test_thread_local.c
	./test_thread_local

# Fuzzing the parser. libFuzzer ships with clang, so this uses clang whatever
# CC is set to; override with FUZZ_CC if yours lives elsewhere.
FUZZ_CC ?= clang
FUZZ_SECONDS ?= 60

fuzz: tests/fuzz_load_buffer.c include/ci18n.h
	$(FUZZ_CC) $(CFLAGS) $(EXTRA_CFLAGS) $(CPPFLAGS) -g \
		-fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
		-o fuzz_load_buffer tests/fuzz_load_buffer.c

# The first directory is where libFuzzer writes what it finds, the second is
# read only, so the committed seeds stay as they are.
fuzz-run: fuzz
	mkdir -p fuzz_findings
	./fuzz_load_buffer fuzz_findings tests/fuzz_corpus \
		-max_total_time=$(FUZZ_SECONDS) -print_final_stats=1

# Replays one input without libFuzzer, so a crash found by CI can be
# reproduced with any compiler. INPUT names the file.
fuzz-replay: tests/fuzz_load_buffer.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) -DCI18N_FUZZ_REPLAY -g -o fuzz_replay tests/fuzz_load_buffer.c
	./fuzz_replay $(INPUT)

# Replays the whole seed corpus. Useful as a plain regression check on a
# toolchain that has no libFuzzer at all.
fuzz-corpus: tests/fuzz_load_buffer.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) -DCI18N_FUZZ_REPLAY -g -o fuzz_replay tests/fuzz_load_buffer.c
	@for input in tests/fuzz_corpus/*; do ./fuzz_replay "$$input" || exit 1; done

# $(RM) is `rm -f`, which make runs through its shell. Both name variants are
# listed because MinGW gcc appends .exe to an extensionless -o target.
#
# Do not switch on $(OS) here: it reads Windows_NT under Git Bash too, and the
# cmd.exe form of this rule then leaves a junk file named `nul` in the tree.
clean:
	$(RM) example example.exe test_ci18n test_ci18n.exe \
		test_thread_local test_thread_local.exe \
		fuzz_load_buffer fuzz_load_buffer.exe fuzz_replay fuzz_replay.exe
