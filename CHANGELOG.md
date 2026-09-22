# Changelog

Notable changes to ci18n. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Because this is a single-header library, upgrading means replacing one file.
Check `CI18N_VERSION` at compile time if you need a specific version:

```c
#if CI18N_VERSION < CI18N_VERSION_NUMBER(1, 0, 0)
#error "ci18n 1.0.0 or newer is required"
#endif
```

## 1.0.0 - 2026-09-22

First tagged release.

### Added

- Key and value storage for multiple languages, with a fallback language
- `ci18n_load_language()` for files and `ci18n_load_from_buffer()` for
  translations compiled into the binary
- `key=value` format with `#` and `;` comments, blank lines, surrounding
  whitespace trimmed, and a UTF-8 BOM skipped
- Runtime entry management: `ci18n_set()`, `ci18n_remove()`, `ci18n_clear()`,
  `ci18n_count()`, `ci18n_has()`
- Diagnostics: `ci18n_last_error()`, `ci18n_error_string()` and
  `ci18n_last_load_stats()`, which reports lines read, entries loaded, lines
  skipped, malformed lines with the first offending line number, and anything
  truncated
- `CI18N_THREAD_LOCAL_CONTEXT` for one context per thread
- `CI18N_DEF` for static or `dllexport` and `dllimport` builds
- `CI18N_VERSION_*` macros and a compile-time version check
- Configurable limits: `CI18N_MAX_KEY_LENGTH`, `CI18N_MAX_VALUE_LENGTH`,
  `CI18N_MAX_LANGUAGES`, `CI18N_MAX_KEYS_PER_LANGUAGE`,
  `CI18N_MAX_LINE_LENGTH`, `CI18N_MAX_CODE_LENGTH`

### Notes on the code before this tag

There was never a tagged or published release before this one, so nothing here
breaks a version anyone could have depended on. The list is worth keeping
anyway: if you vendored the header from this repository before 2026-09-22, you
have a copy with these defects.

- An unchecked `malloc()` left the entry capacity at 64 over a null pointer, so
  the next write went through it. The only crash path in the library.
- A UTF-8 BOM made the first key of a file unreachable: it was stored with the
  BOM bytes prefixed to its name.
- Whitespace trimming used the locale-dependent `isspace()`, which can classify
  bytes above 127 as space and eat the tail of a UTF-8 sequence.
- `CI18N_THREAD_SAFE` selected thread-local storage by operating system. MinGW
  gcc defines `_WIN32` but ignores `__declspec(thread)` with a warning, so
  every thread silently shared one global context.
- A language code longer than the code field was truncated on write but
  compared in full on lookup, so the value could be written and never read
  back, and a second code sharing the prefix created a duplicate, permanently
  unreachable language.
- `ci18n_get_languages()` returned a pointer to a function-local `static`
  array, which was neither thread-local nor reentrant.
- `CI18N_MAX_LINE_LENGTH` equalled `CI18N_MAX_VALUE_LENGTH`, so an over-long
  value was always cut by the line limit, and the tail of the cut line came
  back as a separate line with no separator and was counted as malformed.
- Nothing reported why a call failed, and a load returned success for a file
  whose every line was malformed.

`CI18N_THREAD_SAFE` still works as an alias for
`CI18N_THREAD_LOCAL_CONTEXT`, with a deprecation note at compile time.

### Verified by

- 59 unit tests, covering the file loader, every argument guard, every
  configured limit and every error code
- The suite built and run at `-std=c99`, `-std=c11` and `-std=c17`, on gcc and
  clang
- A thread test that checks concurrent threads really do get separate contexts
- AddressSanitizer with leak detection, UndefinedBehaviorSanitizer and
  ThreadSanitizer, on gcc and clang
- 573,000 fuzzed parser inputs with no crash, and a seed corpus of the inputs
  that have broken this parser before
