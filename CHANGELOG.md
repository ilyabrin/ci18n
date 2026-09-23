# Changelog

Notable changes to ci18n. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

Because this is a single-header library, upgrading means replacing one file.
Check `CI18N_VERSION` at compile time if you need a specific version:

```c
#if CI18N_VERSION < CI18N_VERSION_NUMBER(2, 6, 0)
#error "ci18n 2.6.0 or newer is required"
#endif
```

## Unreleased

### Added

- Numbers written the way each language writes them. `{n:number}` in a
  translation, or `ci18n_format_number()` directly, turns `"1234567.5"` into
  1,234,567.5, 1.234.567,5, 1 234 567,5 or 12,34,567.5. `{n:number,2}`
  rounds or pads to two fraction digits, half to even. Separators, minus
  sign, minimum grouping and Indian grouping come from CLDR 48 for all 71
  languages with plural rules, in under 1 KB; `tools/cldr_numbers.py`
  generates the table and the 803 cases the tests check it against. A
  formatter registered as `number` replaces the built-in one.

## 2.13.0 - 2026-09-23

### Added

- `ci18n_load_mo()` and `ci18n_load_mo_from_buffer()`, with `_in`
  variants, load compiled gettext catalogues directly: no libintl, no
  conversion step. Contexts become key prefixes and plural forms land on
  CLDR categories, the same way `tools/po2ci18n.py` maps them. Both byte
  orders are read and every offset is checked against the size. About
  2.5 KB of code; `CI18N_NO_MO` leaves it out. `make test-mo` checks the
  loader against the converter on files compiled by `msgfmt`, and the
  fuzzer now runs it too.

- Bidi isolation. `ci18n_set_bidi_isolation(true)` wraps every value
  `ci18n_format` and its relatives fill in with FSI and PDI, so an English
  name in an Arabic sentence, or a Hebrew one in English, no longer
  reorders the text around it. Per catalogue, off by default.
  `ci18n_bidi_isolate()` does the same for one string, `ci18n_bidi_mark()`
  gives LRM or RLM for a direction, and `CI18N_FSI`, `CI18N_PDI`,
  `CI18N_LRM` and `CI18N_RLM` are the characters themselves.

### Fixed

- A result cut short by `ci18n_format` and its relatives could skip a
  stretch of text: when a partial character was dropped at the end of the
  buffer, a shorter piece after it still went into the bytes left free.
  "{v}!" with v = "aб" in 3 bytes gave "a!". Output now stops at the first
  cut, so it is always a prefix of the full result.

## 2.12.0 - 2026-09-23

### Added

- `ci18n_foreach()` and `ci18n_foreach_in()` visit every entry of a
  language, for tools that need keys they did not know in advance: a
  checker, an exporter, a debug dump.
- Three examples, each with a README: a CI checker for translation files, a
  threaded server that reloads translations under readers, and a small
  device's screen in three languages, one of them right-to-left. `make
  examples` and ctest build them and compare their output with the output
  they are known to give.

### Fixed

- A plural or ordinal form taken from the fallback language was chosen by
  the current language's rules. With Arabic current and English as the
  fallback, 3 came out as "3th": Arabic has a single ordinal form, so it
  asked for `[other]`. Each language is now tried with its own rules, the
  current one first. A side effect: a plain `key` in the current language
  now wins over a `key[form]` in the fallback, since it is in the right
  language.

### Changed

- Reads in the shared threading mode scale across cores. A catalogue holds
  16 read-write locks on separate cache lines, and each thread reads through
  its own, so eight threads do 162 million `ci18n_get` calls a second instead
  of 14 million. Writers take all 16 locks, in order. The catalogue struct is
  about 1 KB larger in this mode, and its `lock` field is now `locks`.

## 2.11.0 - 2026-09-23

### Added

- Benchmarks. `make bench` times lookups, formatting and loading, and
  reports memory; `make bench-threads` shows how a shared catalogue scales;
  `make bench-gettext` runs the same lookups through glibc gettext. CMake
  builds `ci18n_bench` too. The READMEs now carry the numbers, along with
  code size and memory use.

### Changed

- A loaded language takes about 1.4 times the size of its file in memory,
  down from 2 to 2.5 times: the loaders give back the spare room their
  buffers grew into. Loading gets about 20% slower for the one copy this
  takes. A later `ci18n_set()` grows the buffers by doubling, as before.
- `ci18n_plural_category()` and `ci18n_ordinal_category()` are about six
  times faster, 90 ns down to 15 ns, and `ci18n_format_plural()` about twice
  as fast. The rule tables were searched with a length check and case
  folding on every row; the code is now folded once.

### Documentation

- The shared threading mode does not scale reads across cores: every reader
  updates the same lock. The READMEs now say so, with numbers.

## 2.10.0 - 2026-09-23

### Added

- Ordinals. `ci18n_format_ordinal()`, `ci18n_ordinal()`,
  `ci18n_ordinal_or_key()` and `ci18n_ordinal_category()`, with `_in`
  variants, choose between `place[one]`, `place[two]`, `place[few]` and
  `place[other]` by CLDR 48's ordinal rules: 1st, 2nd, 3rd, 4th, and 11th,
  12th, 13th. 71 languages, and an unknown one has no ordinal forms, which
  is what most languages have anyway.
- `CI18N_ERR_LAST`, the highest error code, so code walking every code needs
  no edit when one is added. A macro rather than an enumerator, so exhaustive
  switches over `ci18n_error_t` are not asked to handle something that is not
  an error.
- `tools/cldr_samples.py` generates `tests/cldr_samples.h` from CLDR's own
  sample numbers, and the unit tests check every supported language against
  it. `make cldr-samples` regenerates it; when CLDR releases, bump the pinned
  version and read the diff.

### Fixed

Eight languages had cardinal rules that disagreed with CLDR, found by that
check. A translation that never gave the missing form is unaffected, since
lookup falls back to `[other]`.

- **Hebrew** has a dual, so 2 is `two`, not `other`. A `key[two]` form was
  never selected. `iw`, the old code, is covered too.
- **Marathi**'s 0 is `other`, not `one`.
- **Sinhala**'s 0 is `one`, not `other`.
- **French, Portuguese, Spanish, Italian and Catalan** give a whole number of
  millions its own `many` form: "1 000 000 de fichiers", where "de" is
  required. It was `other`.

## 2.9.0 - 2026-09-23

### Added

- Pluggable formatters. A translation can write `{created:date,long}`, and the
  function you registered with `ci18n_set_formatter()` renders the value. The
  library parses the placeholder, finds the formatter and does the buffer
  arithmetic; rendering dates, numbers and currencies stays in your code,
  where the locale knowledge you already have lives.
- Everything after the first comma is the formatter's argument, verbatim, so a
  formatter defines its own syntax there.
- `ci18n_format_in()` and `ci18n_format_plural_in()`, so an explicit catalogue
  can be formatted at all. Their absence was an oversight in 2.6.0, and it
  would have made per-catalogue formatters unreachable.
- `ci18n_remove_formatter()`, and `_in` variants of both registration calls.
- Two error codes: `CI18N_ERR_UNKNOWN_FORMATTER` for a translation naming a
  formatter nobody registered, and `CI18N_ERR_TOO_MANY_FORMATTERS` for
  `CI18N_MAX_FORMATTERS`.
- `CI18N_MAX_FORMATTERS`, `CI18N_MAX_FORMATTER_NAME` and
  `CI18N_MAX_FORMATTER_ARG` to size all of that.

### Changed

- A placeholder naming an unregistered formatter is left visible and reported,
  rather than dropped. The rest of the sentence still renders, because a
  missing formatter is a reason to see a defect, not to lose the text around
  it.
- Formatters belong to a catalogue and are not inherited from the default one.
  Inheritance would have meant formatting sometimes took two catalogues' locks
  at once, which is a deadlock waiting for the right interleaving.
- A placeholder containing a colon used to be a name with a colon in it, which
  matched nothing and was left visible. It is now a name and a formatter.
  Output is unchanged unless you register a formatter, since an unknown one is
  also left visible, but `ci18n_last_error()` now reports it.

## 2.8.0 - 2026-09-23

Not released on its own. Its commit failed the MSVC build, and the fix
arrived with 2.9.0 an hour later, so there is no v2.8.0 tag: everything
below ships in 2.9.0.

### Added

- UTF-8 helpers: `ci18n_utf8_valid()`, `ci18n_utf8_length()` for characters
  rather than bytes, `ci18n_utf8_sequence_length()` for stepping a string a
  character at a time, and `ci18n_utf8_truncate()` for cutting text to fit a
  buffer without splitting a character.
- Validation is strict in the sense the standard requires: overlong
  encodings, surrogate halves and values above U+10FFFF are rejected, not
  tolerated.

### Fixed

- **`ci18n_format()` and `ci18n_format_plural()` could produce invalid UTF-8.**
  Truncation cut at whatever byte the buffer ran out on, which for any
  non-ASCII text lands inside a character about as often as not. Asking for a
  12-byte Russian greeting in an 8-byte buffer returned 7 bytes ending in a
  lone lead byte. They now cut at a character boundary, so a truncated result
  is still decodable, which means it can be shorter than `capacity - 1`. The
  reported length is unchanged, so buffer sizing works as before.

  A property test over 20000 random inputs at every capacity, about 1.28
  million calls, reports no invalid output; the same test against the previous
  code reports 495588.

## 2.7.0 - 2026-09-23

### Added

- Text direction, so an Arabic or Hebrew interface can lay itself out:
  `ci18n_direction()` for any language code, `ci18n_current_direction()` for
  the current one, and `ci18n_direction_name()` giving the "ltr" or "rtl" that
  HTML's `dir` attribute and CSS's `direction` property expect.
- A script subtag decides direction on its own, because direction belongs to
  the script rather than the language. So `az-Arab` is right to left although
  `az` is not, and romanized `ar-Latn` is left to right although Arabic is
  not.

### Fixed

- Language codes now match case-insensitively, as BCP 47 says they should.
  `ci18n_plural_category("RU", 2)` used to miss the Russian rule and fall
  through to the English one, returning `other` where `few` was correct. Any
  code reaching the library upper-cased, which is common for codes that came
  from an environment variable, was affected.

## 2.6.2 - 2026-09-22

### Fixed

- The header now compiles clean under the strictest warnings each compiler
  offers, which it did not before: MSVC at `/W4` reported one C4996 for
  `fopen`, and a single warning from somebody else's header blocks any project
  built with warnings as errors.

  Suppressed with `#pragma warning(push/disable/pop)` around that one call
  rather than by defining `_CRT_SECURE_NO_WARNINGS`. That macro covers the
  whole translation unit, so the library would have been silencing warnings
  about the caller's code as well as its own. The warning does not apply in
  substance either: the return value is checked and nothing unbounded is
  written.

  `fopen_s` was the other option and was rejected: it adds a platform branch
  to the loader and makes it harder to read, for no gain in safety.

- The test suite used `sprintf`, which MSVC also deprecates. Changed to
  `snprintf`, so no suppression is needed there at all.

### Added

- A CI step that rebuilds with warnings as errors, `/W4 /WX` on MSVC and
  `-Wall -Wextra -Wpedantic -Werror` elsewhere, on every compiler. Staying
  warning-clean is only true for as long as something checks.

  The step needs `MSYS_NO_PATHCONV=1`, because Git Bash rewrites a leading
  slash into a Windows path and `/W4` arrives at cmake as
  `C:/Program Files/Git/W4`. Worth recording because the failure looks
  nothing like its cause.

## 2.6.1 - 2026-09-22

### Fixed

- **The shared threading mode provided no mutual exclusion on macOS.** The
  default catalogue's lock was never statically initialised, only zeroed.
  `PTHREAD_RWLOCK_INITIALIZER` happens to be all zeros on glibc, so it worked
  there by accident; on macOS the initialiser carries a signature, a zeroed
  lock is invalid, and every `pthread_rwlock_rdlock` returned `EINVAL` and did
  nothing. Readers saw torn state immediately, which is how CI caught it.

  `CI18N_RWLOCK_INIT` existed all along and was simply never used.

- Lock failures now abort with a message instead of being ignored. An rwlock
  call only fails when the lock is unusable, and continuing then means running
  with no mutual exclusion, which is exactly how the bug above stayed hidden
  on one platform while passing on two others. Define `CI18N_LOCK_FAILED` to
  handle it differently.

This is the second bug of this shape in the project: a platform where the
mechanism silently does nothing rather than refusing. The first was
`__declspec(thread)` being ignored by MinGW gcc. Both were found by running
the mode on more than one platform, not by reading the code.

## 2.6.0 - 2026-09-22

Explicit catalogues, so ci18n can be used inside a library.

### Added

- `ci18n_create()`, `ci18n_destroy()` and `ci18n_default()`, plus an `_in`
  variant of every function that touches a catalogue. The plain names are
  unchanged and are those variants applied to a default catalogue, so nothing
  downstream needs editing.

  ```c
  ci18n_t *ui = ci18n_create();
  ci18n_load_language_in(ui, "en", "en.txt");
  ci18n_set_current_in(ui, "en");
  puts(ci18n_get_or_key_in(ui, "greeting"));
  ci18n_destroy(ui);
  ```

  The point is not convenience. Until now a library using ci18n internally
  shared one current language with the application that linked it, and
  whichever called `ci18n_set_current()` last won. That made ci18n unusable
  inside any reusable component, and no amount of documentation could fix it.

- A catalogue carries its own languages, its own current and fallback
  selection, and in the shared threading mode its own lock, so two catalogues
  never wait on each other.

- `ci18n_last_error_in()` and `ci18n_last_load_stats_in()` report what a
  catalogue itself recorded. The plain versions stay per-thread in the shared
  mode, which is what a caller wants.

- `make valgrind`, and a valgrind job in CI. It overlaps AddressSanitizer,
  which already runs, but not exactly: valgrind sees uninitialised reads ASan
  does not, and needs no instrumentation, so it checks the code an ordinary
  build produces.

### Changed

- Internally every helper and every unlocked core now takes the catalogue it
  operates on. That was a separate commit with no behaviour change, because a
  mechanical refactor of that size is only safe if the tests are the judge.
- The rwlock moved into the catalogue, which is where 2.5.0 said it was and
  where per-catalogue locking requires it. Its type therefore moved to the
  public section, so the shared mode pulls in `pthread.h`, or `windows.h` on
  Windows, from there.

### Fixed

- `ci18n_init()` and `ci18n_free()` cleared the catalogue with a `memset` over
  the whole struct, which named the global rather than the catalogue they were
  given. Harmless with one catalogue, wrong the moment there are two. The same
  `memset` also zeroed the lock, which deadlocked the shared tests as soon as
  the lock became a field.
- The glibc guard added in 2.5.0 never fired. It tested whether
  `_POSIX_C_SOURCE` was defined, but `-pthread` makes glibc define it as
  `199506L` while `pthread_rwlock_*` needs `200112L`. It now checks
  `__USE_XOPEN2K`, glibc's own gate, and both suggested remedies are verified.

## 2.5.0 - 2026-09-22

Real thread safety: one shared context behind a reader-writer lock.

### Added

- `CI18N_THREAD_SHARED`, a third threading mode. There are now three, and the
  README spells them out:

  1. Default. One global context, no locking. Safe when every load finished
     before the threads started and they only read afterwards.
  2. `CI18N_THREAD_LOCAL_CONTEXT`. A context per thread. Isolation, not
     sharing: each thread loads its own translations.
  3. `CI18N_THREAD_SHARED`. One shared context behind an rwlock, which is the
     "load once, read from many threads, reload occasionally" case. Readers do
     not block each other; a writer excludes everyone.

  Defining both macros is an error rather than a coin toss.

- `ci18n_get_copy()`, which copies a translation into a buffer you own. This
  is the safe read under `CI18N_THREAD_SHARED`, because the copy is made while
  the read lock is still held. A returned pointer cannot be made safe by a
  lock: the instant the lock is released, a writer may reallocate the storage
  it points into. Useful single-threaded too, whenever a translation has to
  outlive the next write.

- In the shared mode, `ci18n_last_error()` and `ci18n_last_load_stats()` are
  per-thread rather than shared. A diagnostic describes the call that produced
  it, so one shared slot would mean two threads overwriting each other and
  neither being able to trust the answer.

- `make test-shared` runs four readers and a writer against one context at the
  same time. It runs in CI both plainly and under ThreadSanitizer, which is
  the only way an rwlock claim can be believed. Verified against a build with
  the locks stubbed out, where ThreadSanitizer reports the race immediately.

### Changed

- Every public function is now a thin locking wrapper around an unlocked core.
  Wrappers rather than locks threaded through the bodies: these functions have
  several early returns each, and one lock and unlock pair per function cannot
  leak the lock down a path somebody forgets. The internal composition was
  rerouted to the cores, since neither `pthread_rwlock_t` nor `SRWLOCK` is
  recursive and `ci18n_has()` calling the public `ci18n_get()` would have
  deadlocked.
- The lock lives inside the context rather than beside the global, so that
  when the API grows an explicit handle it travels with it and each handle
  locks itself.

### Notes

On glibc, `CI18N_THREAD_SHARED` needs `-D_POSIX_C_SOURCE=200809L`, or
`-std=gnu99` in place of `-std=c99`: strict ANSI mode hides the POSIX
threading declarations. The header says so with an `#error` rather than
letting `pthread_rwlock_rdlock` arrive as an implicit declaration.

## 2.4.0 - 2026-09-22

Unloading languages, and a converter for gettext catalogues.

### Added

- `ci18n_remove_language()` unloads a language entirely, freeing its memory
  and releasing its slot. `ci18n_clear()` empties a language but keeps it
  loaded, so a program switching between many languages over a long run would
  eventually fill `CI18N_MAX_LANGUAGES` with empty ones. If the language being
  removed is the current or the fallback one, that selection is cleared:
  pointing at a language that no longer exists is worse than pointing at
  nothing.

- `tools/po2ci18n.py` converts a gettext `.po` catalogue into a ci18n
  translation file. It handles msgid and msgstr with C escapes and the
  adjacent-string continuation gettext uses for long entries, msgid_plural
  with its indexed forms mapped onto CLDR category names, msgctxt as a key
  prefix, and it skips fuzzy, untranslated and obsolete entries.

  A converter rather than a `.po` reader in the header: a full reader is
  around 700 lines, 300 of them an evaluator for the C expression in gettext's
  Plural-Forms header, which would double the parse and fuzz surface of a
  library whose point is one small header. Converting offline also evaluates
  that expression on a development machine rather than on a device, and lets
  keys be renamed from English sentences into identifiers with `--keys=slug`.

- `make test-po` converts a sample catalogue and loads the result back, so the
  converter cannot rot unnoticed. It runs in CI.

### Fixed

- **Escape decoding was switched off for the value of any entry whose key
  contained a backslash.** One flag was doing two jobs: marking the key as
  already decoded also stopped the value being decoded. So `we\\=ird=a\\nb`
  stored a literal `a\\nb` rather than a line break.

  The unit tests missed it because none of them had escapes on both sides of
  the separator at once. The round trip through a real `.po` file caught it on
  its first run, which is the argument for keeping that check in CI.

## 2.3.0 - 2026-09-22

Escape sequences in translation files. Additive.

### Added

Seven escapes, decoded by the file and buffer parsers:

| Written | Becomes |
| --- | --- |
| `\\n` | line feed |
| `\\t` | tab |
| `\\r` | carriage return |
| `\\\\` | backslash |
| `\\=` | an equals sign, so a key can contain one |
| `\\#` and `\\;` | a key may start with a comment marker |
| `\\ ` | a space that trimming will not eat |

Before this, a value was taken literally, so a translation could not contain a
line break at all.

### Notes

- **Only the parsers decode.** `ci18n_set()` receives strings the C compiler
  has already unescaped, so decoding there would turn `"C:\\new"` into a path
  with a line break in it.
- The separator is the first *unescaped* equals sign, so `we\\=ird=value`
  stores the key `we=ird`.
- An unrecognised sequence keeps both characters: `\\q` stays `\\q`, and a
  trailing backslash stays a backslash. Same reasoning as an unmatched
  placeholder: a mistake in a translation should be visible rather than
  silently eat the character after it.
- Two escape-heavy seeds were added to the fuzz corpus, which is now 20 files,
  and the random stress inputs now include backslashes and braces rather than
  reaching them only by chance.

## 2.2.0 - 2026-09-22

Named interpolation. Additive, as before.

### Added

- `ci18n_format()` fills named placeholders from name and value pairs:

  ```ini
  greeting=Hello, {name}! You have {count} messages.
  greeting=Привет, {name}! У вас {count} сообщений.
  ```

  ```c
  char text[256];
  ci18n_format(text, sizeof(text), "greeting", "name", user, "count", "3", NULL);
  ```

  Names rather than positions, because the order values appear in differs
  between languages and that is exactly what positional formatting cannot
  express. Write `{{` and `}}` for literal braces.

- `ci18n_format_plural()` selects the plural form for a count and fills the
  placeholders in one call, with the count available as `{count}` without
  being passed as a pair. An explicit pair of that name still wins, so a
  caller can render "99+" while still selecting the right form.

### Notes

- Both follow `snprintf()`: at most `capacity - 1` bytes are written, the
  result is always terminated, and the return value is the length the whole
  result would have had. `ci18n_format(NULL, 0, ...)` measures without
  writing.
- Values are strings, never a format string. A translation file is data,
  often not written by the programmer, and handing it to `printf()` as a
  format makes every translator a potential attacker.
- A placeholder with no matching name is left exactly as written, so a typo
  in a translation is visible in the output rather than a silent hole.
- The formatter is now part of the fuzz target, since it parses translation
  text that came from a file. Three brace-heavy seeds were added to the
  corpus.

## 2.1.0 - 2026-09-22

Plural rules and locale detection. Both additive: nothing existing changes.

### Added

- `ci18n_plural()` and `ci18n_plural_or_key()`, which pick a translation by
  count instead of by key alone. Plural forms are ordinary keys with the
  category in brackets, so the file format and the parser are untouched:

  ```ini
  files[one]=%d файл
  files[few]=%d файла
  files[many]=%d файлов
  ```

  Lookup tries `key[category]`, then `key[other]`, then plain `key`, so a
  translation only has to be as detailed as it needs to be.

- `ci18n_plural_category()` and `ci18n_plural_category_name()`, for callers
  that want the category itself.

- CLDR plural rules for 13 families, covering: English-like one and other;
  French and Portuguese, where zero counts as one; Russian, Ukrainian and
  Belarusian; Polish; Czech and Slovak; Croatian and Serbian; Arabic, which
  uses all six categories; Lithuanian; Latvian; Slovenian; Irish; Romanian;
  and the languages with no plural distinction at all, such as Japanese,
  Chinese and Korean. An unknown language is treated as English-like rather
  than refused.

- `ci18n_detect_locale()`, which reads `LC_ALL`, `LC_MESSAGES` and `LANG`, and
  on Windows asks the system for the user's default locale name. The result is
  normalised: `ru_RU.UTF-8` arrives as `ru-RU`, and `C` or `POSIX` report
  nothing because they name no language. Define `CI18N_NO_PLATFORM_LOCALE` to
  keep `windows.h` out of the build and use only the environment.

- `ci18n_set_current_best()`, which selects the best loaded language for a
  locale by dropping subtags as it goes: `ru-RU` then `ru`. So a program can
  ship a plain `ru` translation and still honour a user asking for Russian as
  spoken in Russia. A failed selection leaves the current language alone,
  rather than leaving the program with none.

- Plural forms in the bundled translation files, and both features in the
  example.

### Notes

Only integer counts are considered. CLDR distinguishes 1 from 1.0 in some
languages; this does not. Negative counts use their absolute value, since
minus three things is still three things.

## 2.0.0 - 2026-09-22

Same API, rebuilt storage. Every function keeps its signature and its
behaviour; what changed is what the library costs.

### Changed

- **Keys and values are packed into a per-language arena** and referenced by
  offset instead of living in fixed 256 and 4096 byte fields. An entry is now
  16 bytes rather than 4352.
- **Lookups go through a hash table** (FNV-1a, chained, power-of-two buckets,
  cached hashes) instead of walking every key with `strcmp`.
- **Nothing is allocated until the first insert.** An untouched language costs
  only its slot in the context.
- **The parser no longer copies.** It hands the arena slices of the caller's
  line, which also removes a 256 byte and a 4096 byte buffer it kept on the
  stack for every single line.
- `ci18n_remove()` now moves the last entry into the hole rather than shifting
  the rest down, so entry order is no longer insertion order. Nothing
  observable depended on it.

Measured on the same machine, old against new:

| | 1.0.0 | 2.0.0 | |
| --- | --- | --- | --- |
| One entry | 4352 B | 16 B | 272x smaller |
| Three translation files, 24 entries | 835,584 B | 1,600 B | 522x smaller |
| One full language, 1024 entries | 4,456,448 B | 90,112 B | 49x smaller |
| One lookup among 1024 keys | 1860 ns | 75 ns | 25x faster |
| Context, static | 1944 B | 3224 B | 1.3 KB larger |

The context grew because a language descriptor carries three pointers now.
That is 1.3 KB of static memory against 834 KB of heap saved on a realistic
load, which is a trade worth making, particularly on the small device this
library was written for.

### Breaking

Only for code that reached into the structures. `ci18n_entry_t`,
`ci18n_language_t` and `ci18n_context_t` all changed shape, and
`ci18n_entry_t` no longer has `.key` and `.value` as arrays. They are visible
because `ci18n_get_context()` hands the context out, not because their layout
was ever a promise. Code that only calls the functions needs no changes at
all: recompiling against the new header is enough.

`<stdint.h>` is now included.

### Verified by

- The 64 unit tests, five of them new and specific to the storage layer:
  every key reachable after repeated rehashing, removal that keeps the rest
  findable, values updated both shorter and longer, clear and reuse, and a
  file loaded twice
- AddressSanitizer with leak detection, UndefinedBehaviorSanitizer and
  ThreadSanitizer
- 3000 randomly generated parser inputs under AddressSanitizer
- MSVC, gcc and clang, at c99, c11 and c17

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
