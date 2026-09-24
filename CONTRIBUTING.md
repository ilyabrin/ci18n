# Contributing to ci18n

Thanks for taking the time. This is a small single-header library, so the
process is deliberately light.

## Build and test

You need a C99 compiler and make. Nothing else.

```bash
make            # build the example, then build and run the tests
make test       # tests only
make clean
```

Every target, and what it needs beyond a C compiler:

| Target | Does | Needs |
| --- | --- | --- |
| `make test` | The unit tests | |
| `make examples` | Build the three examples and compare their output | pthreads |
| `make test-threads` | Thread-local mode tests | pthreads |
| `make test-shared` | Shared mode tests, readers against a writer | pthreads |
| `make test-po` | `.po` converter round trip | python3 |
| `make test-mo` | `.mo` loader against the converter, both byte orders | python3, `msgfmt` |
| `make test-compiled` | Compiled catalogues against the same files loaded, both threading modes, and the build errors they promise | pthreads |
| `make test-avr` | The tests on 8-bit AVR under QEMU, catalogues in flash on an Arduino Uno, and the build error `ci18n_get()` has to give there | avr-gcc, avr-libc, `qemu-system-avr` |
| `make fuzz-run` | Fuzz the parsers, see below | clang with libFuzzer |
| `make fuzz-corpus` | Replay the fuzz seeds | |
| `make bench`, `bench-threads`, `bench-gettext` | Timings, see [bench/README.md](bench/README.md) | pthreads; glibc for gettext |
| `make cldr-samples`, `cldr-numbers` | Regenerate the CLDR tables | python3, a network |
| `make install`, `uninstall` | The header and a pkg-config file | |

A module left out with a `CI18N_NO_*` macro is tested by passing it in:
`make test EXTRA_CFLAGS=-DCI18N_MINIMAL`. CI does that for each one.

Build with warnings as errors before you open a PR, because CI does:

```bash
make clean && make EXTRA_CFLAGS=-Werror
```

The header is expected to stay clean at the strictest level each compiler
offers, `-Wall -Wextra -Wpedantic -Werror` and `/W4 /WX`, in all three
threading modes. That is not pedantry: one warning from somebody else's header
blocks any project built with warnings as errors. CI rebuilds that way on
every compiler.

Where a warning has to be silenced, silence it around the single line that
causes it, never by defining something like `_CRT_SECURE_NO_WARNINGS`. That
macro covers the whole translation unit, so the library would be deciding
warning policy for code it did not write.

Try the other compiler too if you have it. CI covers gcc and clang on Linux and
macOS, and MinGW gcc on Windows:

```bash
make clean && make CC=clang EXTRA_CFLAGS=-Werror
```

### Thread-local context tests

`make test-threads` builds [tests/test_thread_local.c](tests/test_thread_local.c)
with `CI18N_THREAD_LOCAL_CONTEXT` and checks that concurrent threads really do
get separate contexts. It needs pthreads, so it is not part of `make`.

```bash
make test-threads
```

### Other standards

The header has branches that only compile under a newer standard, so CI sweeps
all three. `CSTD` selects one without restating the warning flags:

```bash
make clean && make CSTD=c11 EXTRA_CFLAGS=-Werror
```

### Sanitizers

CI runs the suite, the example and the thread tests under
AddressSanitizer + UndefinedBehaviorSanitizer and under ThreadSanitizer. Worth
running locally before touching the parser or anything that allocates:

```bash
make clean
make test EXTRA_CFLAGS="-fsanitize=address,undefined -fno-sanitize-recover=all -g"
```

Note that MinGW ships without the sanitizer runtimes, so on Windows this needs
WSL, MSVC or a Linux box.

### CMake

The CMake build is the one that covers MSVC, and the only thing that checks
the packaging actually works:

```bash
cmake -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

With a multi-config generator, Visual Studio being the usual one, both steps
need the configuration naming: `cmake --build build --config Release` and
`ctest --test-dir build -C Release`.

Packaging is only real if something consumes it, so
[tests/integration/](tests/integration/) holds two consumers, one against an
install tree and one as a subproject, and CI runs both:

```bash
cmake --install build --prefix /tmp/prefix
cmake -B build-find -S tests/integration/find_package -DCMAKE_PREFIX_PATH=/tmp/prefix
cmake --build build-find && ctest --test-dir build-find

cmake -B build-sub -S tests/integration/subproject
cmake --build build-sub && ctest --test-dir build-sub
```

### Fuzzing

The parser takes bytes the program did not write, so it is fuzzed as well as
unit tested. [tests/fuzz_load_buffer.c](tests/fuzz_load_buffer.c) drives
`ci18n_load_from_buffer()`, then reads everything back through the public API,
using the same input as lookup keys. It also asserts that the load stats add
up, so a miscounted line is a crash rather than a wrong number nobody checks.

libFuzzer comes with clang:

```bash
make fuzz-run                  # 60 seconds by default
make fuzz-run FUZZ_SECONDS=600
```

Seeds live in [tests/fuzz_corpus/](tests/fuzz_corpus/) and are the inputs that
have caused trouble here before: a BOM, CRLF, a lone CR, an over-long line, an
over-long key, malformed lines, binary bytes. Findings are written to
`fuzz_findings/`, so the committed seeds are never modified. Add a seed
whenever you fix a parser bug.

To reproduce one input, with any compiler and no libFuzzer:

```bash
make fuzz-replay INPUT=tests/fuzz_corpus/long_line
make fuzz-corpus   # replays every seed
```

`make fuzz-corpus` is the fallback check on a toolchain without libFuzzer,
which includes MinGW.

## Adding a test

Tests live in [tests/test_ci18n.c](tests/test_ci18n.c) and use a few macros at
the top of that file. There is no framework and no discovery step, so a new test
takes two edits:

```c
TEST(test_my_new_thing)
{
    ci18n_init();

    ASSERT(ci18n_set("en", "k", "v") == true);
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("k"), "v");

    ci18n_free();
}
```

Then register it in `main()`:

```c
RUN_TEST(test_my_new_thing);
```

Notes that will save you time:

- `ASSERT` and `ASSERT_STR_EQ` return from the test body on failure, so put
  `ci18n_free()` cleanup in mind: a failing test leaks, which is fine because
  the process is about to exit.
- Always `ci18n_init()` at the start and `ci18n_free()` at the end. The context
  is global, so a test that skips cleanup will contaminate the next one.
- The suite exits non-zero if anything fails. That is what CI checks.

### Plural and ordinal rules

The rules come from CLDR, and the tests hold them to it:
[tests/cldr_samples.h](tests/cldr_samples.h) is every sample number CLDR
publishes for the supported languages, and `test_rules_match_cldr_samples`
checks each one. The file is generated, so do not edit it by hand:

```sh
make cldr-samples   # fetches the pinned CLDR release and regenerates
```

Adding a language to the cardinal table adds its samples on the next
regeneration. When CLDR releases, bump `CLDR_VERSION` in
[tools/cldr_samples.py](tools/cldr_samples.py), regenerate and read the diff:
a changed sample is a changed rule, and needs a matching change in the header.

## Code style

Match the file you are editing. The existing style is:

- 4 spaces, no tabs
- Allman braces in the header, K&R in the example
- `/* block comments */`, not `//`, so the header stays C89-comment clean
- `ci18n_` prefix on everything with external linkage, including internal
  helpers, since this is a single-header library that lands in someone else's
  translation unit
- Configuration and feature macros are `CI18N_UPPER_SNAKE`

Comments should say why, not what. `ci18n.h` has examples of the level of
detail that is useful: a comment earns its place when it explains a constraint
the code cannot state itself.

## What makes a change easy to merge

- One logical change per PR
- A test for anything that touches the parser or the lookup path
- Public API changes documented in the header, on the page in
  [docs/](docs/) that covers the feature, and in [docs/api.md](docs/api.md)
- The English docs are canonical, and [docs/ru/](docs/ru/) and
  [README.ru.md](README.ru.md) mirror them. `python3 tools/check_docs.py`
  checks that every link resolves and that each Russian page matches its
  English one in sections, code blocks, tables, links and numbers; CI runs
  it too. So a change to an English page needs the same change on the
  Russian one. If you do not write Russian, make it there in English: the
  check passes, and the maintainer translates it

## Public API changes

The API is stable within a major version. If you need to change an existing
signature or behaviour, say so explicitly in the PR description, and bump
`CI18N_VERSION_*` in [include/ci18n.h](include/ci18n.h) in the same change so a
downstream compile-time version check stays meaningful.

## Code of conduct

Be decent. The full text is in
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md), and the short version is that
reports go through a [form](https://docs.google.com/forms/d/e/1FAIpQLSfs68uXGT7UfC5t__Fr1nmCIUMRPfshtF6UM5-bzpcvk-ihHQ/viewform)
that does not ask who you are, rather than a public issue.

## Reporting bugs

Open an issue with the compiler, its version, the platform, and the shortest
input that reproduces it. A translation file or a buffer literal that triggers
the problem is worth more than a description of it.

For anything security relevant, do not open a public issue. See
[SECURITY.md](SECURITY.md).
