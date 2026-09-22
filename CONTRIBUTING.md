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

Build with warnings as errors before you open a PR, because CI does:

```bash
make clean && make EXTRA_CFLAGS=-Werror
```

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
- Public API changes documented in both the header and
  [README.md](README.md)
- The English README is canonical. Updating the Russian
  [README.ru.md](README.ru.md) is welcome but never required

## Public API changes

The API is stable within a major version. If you need to change an existing
signature or behaviour, say so explicitly in the PR description, and bump
`CI18N_VERSION_*` in [include/ci18n.h](include/ci18n.h) in the same change so a
downstream compile-time version check stays meaningful.

## Reporting bugs

Open an issue with the compiler, its version, the platform, and the shortest
input that reproduces it. A translation file or a buffer literal that triggers
the problem is worth more than a description of it.

For anything security relevant, do not open a public issue. See
[SECURITY.md](SECURITY.md).
