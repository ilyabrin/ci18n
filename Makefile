# ci18n build rules
#
#   make           build the example and build-and-run the tests
#   make example   build the example only
#   make test      build and run the unit tests
#   make bench     measure speed and memory, see bench/
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

.PHONY: all clean example test test-threads test-shared test-po valgrind fuzz fuzz-run fuzz-replay fuzz-corpus cldr-samples bench bench-threads bench-gettext

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

# Shared context tests, the CI18N_THREAD_SHARED mode. Needs pthreads, and is
# only meaningful under a thread sanitizer, which CI supplies.
# _POSIX_C_SOURCE because glibc hides pthread_rwlock_* in the strict ANSI mode
# that -std=c99 selects. The header says so too, rather than letting the
# declarations arrive implicitly.
test-shared: tests/test_thread_shared.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) -D_POSIX_C_SOURCE=200809L -pthread \
		-o test_thread_shared tests/test_thread_shared.c
	./test_thread_shared

# Round trip through the .po converter. Needs python3, so it is not part of
# `all`, and the converter lives outside the library on purpose.
PYTHON ?= python3

test-po: tests/test_po_roundtrip.c tools/po2ci18n.py tests/po/sample.po include/ci18n.h
	$(PYTHON) tools/po2ci18n.py tests/po/sample.po -o po_roundtrip.txt
	$(CC) $(ALL_CFLAGS) -o test_po_roundtrip tests/test_po_roundtrip.c
	./test_po_roundtrip po_roundtrip.txt

# Runs the suite under valgrind. Mostly overlapping with AddressSanitizer,
# which CI already runs, but not identically: valgrind sees uninitialised
# reads that ASan does not, and needs no instrumentation, so it also checks
# the code as an ordinary build produces it.
#
# Cannot be combined with a sanitizer build: valgrind and ASan both want to
# own the allocator.
VALGRIND ?= valgrind
VALGRIND_FLAGS ?= --error-exitcode=1 --leak-check=full --show-leak-kinds=all \
	--track-origins=yes --errors-for-leak-kinds=all

valgrind: tests/test_ci18n.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) -g -O0 -o test_ci18n tests/test_ci18n.c
	$(VALGRIND) $(VALGRIND_FLAGS) ./test_ci18n

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

# Regenerate the CLDR sample table the unit tests check the plural and
# ordinal rules against. Fetches the release pinned in the script, so it
# needs a network; the table is committed, so nothing else does.
cldr-samples:
	$(PYTHON) tools/cldr_samples.py

# Benchmarks. Optimised whatever CFLAGS says, since timing a -O0 build tells
# you nothing. Each prints a Markdown table; see bench/README.md.
BENCH_CFLAGS ?= -O2

bench: bench/bench.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) $(BENCH_CFLAGS) -o bench_ci18n bench/bench.c
	./bench_ci18n

# One catalogue shared by 1 to 8 threads. Needs pthreads.
bench-threads: bench/bench_threads.c include/ci18n.h
	$(CC) $(ALL_CFLAGS) $(BENCH_CFLAGS) -D_POSIX_C_SOURCE=200809L -pthread \
		-o bench_threads bench/bench_threads.c
	./bench_threads

# The same lookups through glibc gettext. Linux only, needs msgfmt and the
# ru_RU.UTF-8 locale.
bench-gettext: bench/bench_gettext.c
	$(CC) $(ALL_CFLAGS) $(BENCH_CFLAGS) -o bench_gettext bench/bench_gettext.c
	./bench_gettext

# Installation. There is nothing to compile, so this copies one header and
# generates a pkg-config file next to it.
#
#   make install                     into /usr/local
#   make install PREFIX=$HOME/.local
#   make install DESTDIR=/tmp/stage  staging root for a package build
PREFIX ?= /usr/local
INCLUDEDIR ?= $(PREFIX)/include
PKGCONFIGDIR ?= $(PREFIX)/lib/pkgconfig

# One source of truth: the version comes out of the header it describes.
#
# Two make quirks to avoid here: a `)` in the sed script would close
# $(shell ...) early, and a `#` would start a comment and truncate the line.
# Hence no capture groups and no literal hash.
CI18N_VERSION = $(shell sed -n 's/.*CI18N_VERSION_STRING "//p' include/ci18n.h | tr -d '"')

.PHONY: install uninstall

install: include/ci18n.h ci18n.pc.in
	mkdir -p "$(DESTDIR)$(INCLUDEDIR)" "$(DESTDIR)$(PKGCONFIGDIR)"
	cp include/ci18n.h "$(DESTDIR)$(INCLUDEDIR)/ci18n.h"
	sed -e 's|@PREFIX@|$(PREFIX)|' \
	    -e 's|@INCLUDEDIR@|$(INCLUDEDIR)|' \
	    -e 's|@VERSION@|$(CI18N_VERSION)|' \
	    ci18n.pc.in > "$(DESTDIR)$(PKGCONFIGDIR)/ci18n.pc"
	@echo "installed ci18n $(CI18N_VERSION) to $(DESTDIR)$(INCLUDEDIR)"

uninstall:
	$(RM) "$(DESTDIR)$(INCLUDEDIR)/ci18n.h"
	$(RM) "$(DESTDIR)$(PKGCONFIGDIR)/ci18n.pc"

# $(RM) is `rm -f`, which make runs through its shell. Both name variants are
# listed because MinGW gcc appends .exe to an extensionless -o target.
#
# Do not switch on $(OS) here: it reads Windows_NT under Git Bash too, and the
# cmd.exe form of this rule then leaves a junk file named `nul` in the tree.
clean:
	$(RM) example example.exe test_ci18n test_ci18n.exe \
		test_thread_local test_thread_local.exe \
		fuzz_load_buffer fuzz_load_buffer.exe fuzz_replay fuzz_replay.exe \
		test_po_roundtrip test_po_roundtrip.exe po_roundtrip.txt \
		test_thread_shared test_thread_shared.exe \
		bench_ci18n bench_ci18n.exe bench_threads bench_threads.exe \
		bench_gettext bench_gettext.exe
