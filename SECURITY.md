# Security Policy

## Supported versions

| Version | Supported |
| ------- | --------- |
| 1.0.x   | yes       |

This is a single-header library with no dependencies, so upgrading means
replacing one file. Fixes land on the latest version only.

## Reporting a vulnerability

Please do not open a public issue for a security problem.

Use GitHub's private vulnerability reporting: go to the
[Security tab](https://github.com/ilyabrin/ci18n/security/advisories/new) of
this repository and open a draft advisory. Only the maintainers see it.

Include the compiler and version, the platform, and the shortest input that
reproduces the issue. A translation file or a buffer literal beats a
description. If you have a crash, an ASan or UBSan trace is ideal.

Expect an acknowledgement within a week. Once a fix is ready we publish the
advisory and credit you unless you would rather stay anonymous.

## What ci18n does and does not defend against

Being explicit here is more useful than a blanket promise, because this library
parses files your program may not fully control.

**In scope.** Anything that lets a translation file or buffer corrupt memory,
read out of bounds, or crash the host program. The parser is the main attack
surface: [`ci18n_load_language()`](include/ci18n.h) and
`ci18n_load_from_buffer()` accept arbitrary bytes, and both should fail
gracefully on any input.

**Not vulnerabilities, but known sharp edges.** These are documented behaviour
rather than bugs, though we would like to improve them:

- **Truncation.** A key longer than `CI18N_MAX_KEY_LENGTH`, a value longer than
  `CI18N_MAX_VALUE_LENGTH`, or a line longer than `CI18N_MAX_LINE_LENGTH` is
  truncated. It is counted in `ci18n_last_load_stats()`, but the entry is kept
  in its shortened form rather than rejected.
- **Success means readable, not valid.** `ci18n_load_language()` returns `true`
  for a file whose every line is malformed. It reports only whether the source
  could be read. What was dropped or truncated is available through
  `ci18n_last_load_stats()`, and such a load leaves `ci18n_last_error()` at
  `CI18N_ERR_PARSE`, but a caller that checks only the return value sees
  nothing.
- **Line limit before value limit.** `CI18N_MAX_LINE_LENGTH` and
  `CI18N_MAX_VALUE_LENGTH` are both 4096 by default, so an over-long value is
  cut by the line limit and its tail is then parsed as a separate, malformed
  line.
- **Pointer lifetime.** A `const char *` from `ci18n_get()` is invalidated by
  the next `ci18n_set()` or `ci18n_load_*()` on that language, because the
  entry array is reallocated. Holding one across a load is a use-after-free in
  your code, not in ours.
- **Capacity limits.** Loading more than `CI18N_MAX_LANGUAGES` languages or
  `CI18N_MAX_KEYS_PER_LANGUAGE` keys drops the excess. The call returns false
  with `CI18N_ERR_TOO_MANY_LANGUAGES` or `CI18N_ERR_TOO_MANY_KEYS`, but a load
  in progress keeps going and reports the rest through the load stats.

**Out of scope.** Translation *content*. ci18n hands you back whatever bytes
the file contained, with leading and trailing whitespace trimmed. If you pass a
translated string to `printf()` as a format string, or render it as HTML, that
is your program's escaping problem, and a malicious translation file can
exploit it. Treat translations as untrusted data.

## Hardening advice

If your program loads translation files that a user can replace:

- Validate the path before calling `ci18n_load_language()`
- Never use a translated string as a `printf()` format string
- Escape translated output for whatever you render it into
- Lower `CI18N_MAX_*` to what you actually need, which bounds memory use
