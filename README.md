# ci18n [![CI](https://github.com/ilyabrin/ci18n/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/ci18n/actions)

Lightweight internationalization (i18n) library for pure C projects.

**Other languages:** [Русский](README.ru.md)

## Why this exists

Four reasons, roughly in the order they mattered.

I needed translations in a Telegram bot that runs on a small device. Pulling in
gettext or an XML parser for a few dozen strings was not a trade I wanted to
make. One header, no dependencies and a flat `key=value` file was the shape
that fit.

I wanted to get better at C. Not at reading it, at shipping it: memory that has
to be freed, strings that carry no length, and the parts nobody warns you
about, like what happens to your first key when someone saves the translation
file with a BOM.

I wanted to know how hard this actually is from an engineering standpoint. It
turns out the lookup is the easy half. The hard half is deciding what to do
when the input is wrong, which is why this library now tells you what a load
dropped instead of quietly returning success.

And if it saves somebody else the same afternoon, that is reason enough to put
it out here.

## Features

- ✅ Single header file
- ✅ Pure C, C99 and newer
- ✅ No external dependencies
- ✅ Multiple language support
- ✅ Load from files and buffers
- ✅ Fallback language
- ✅ Thread-local context (optional)
- ✅ UTF-8 compatible, skips a BOM in translation files

Small enough to mean it. Three translation files of eight keys each cost
1600 bytes of heap, and a lookup among a thousand keys takes about 75 ns.
Nothing is allocated until you store something.

## Quick Start

### 1. Include

In **one** .c file of your project:

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

In other files:

```c
#include "ci18n.h"
```

### 2. Initialize

```c
ci18n_init();
ci18n_load_language("en", "translations/en.txt");
ci18n_load_language("ru", "translations/ru.txt");
ci18n_set_current("ru");
ci18n_set_fallback("en");
```

### 3. Usage

```c
printf("%s\n", ci18n_get("welcome_message"));
// or with fallback to key:
printf("%s\n", ci18n_get_or_key("missing_key"));
```

To list what is loaded, pass a buffer you own. The return value is the total
number of languages, which can exceed your capacity:

```c
const char *codes[CI18N_MAX_LANGUAGES];
size_t total = ci18n_get_languages(codes, CI18N_MAX_LANGUAGES);

for (size_t i = 0; i < total; i++) {
    printf("  - %s\n", codes[i]);
}
```

Pass `NULL` to ask for the count alone: `ci18n_get_languages(NULL, 0)`.

### 4. Cleanup

```c
ci18n_free();
```

## Translation File Format

```ini
# This is a comment
greeting=Hello!
farewell=Goodbye!
error=An error occurred
```

## Escape sequences

A value is otherwise taken literally, so these are the way to put a line break
or an equals sign into a translation:

| Written | Becomes |
| --- | --- |
| `\n` | line feed |
| `\t` | tab |
| `\r` | carriage return |
| `\\` | backslash |
| `\=` | an equals sign, so a key can contain one |
| `\#` and `\;` | a key may start with a comment marker |
| `\ ` | a space that trimming will not eat |

```ini
multiline=First line.\nSecond line.
we\=ird=a key with an equals sign in it
padded=keeps one trailing space\
```

Only the file and buffer parsers decode these. `ci18n_set()` takes strings the
C compiler has already unescaped, so `ci18n_set("en", "path", "C:\\new")`
stores a Windows path, not a line break.

An unrecognised sequence keeps both characters, so `\q` stays `\q`. A
mistake in a translation should be visible, not silently eat the character
after it.

## Configuration

Define macros before including the header to configure:

```c
#define CI18N_MAX_KEY_LENGTH 256
#define CI18N_MAX_VALUE_LENGTH 4096
#define CI18N_MAX_LANGUAGES 32
#define CI18N_MAX_KEYS_PER_LANGUAGE 1024
#define CI18N_MAX_CODE_LENGTH 32
#define CI18N_THREAD_LOCAL_CONTEXT  /* one context per thread */
#include "ci18n.h"
```

### Linkage

`CI18N_DEF` decorates every public function. Override it to change how the
library is linked:

```c
#define CI18N_DEF static                  /* keep the API private to one file */
#define CI18N_DEF __declspec(dllexport)   /* export from a Windows DLL */
#define CI18N_DEF __declspec(dllimport)   /* consume that DLL */
```

With `static`, expect `-Wunused-function` for any API you do not call.

### Error handling

Every call that can fail records why, and every call that succeeds clears it:

```c
if (!ci18n_load_language("en", path)) {
    fprintf(stderr, "%s: %s\n", path, ci18n_error_string(ci18n_last_error()));
}
```

A loader returns `true` whenever it could read its source, which says nothing
about the contents. A file whose every line is malformed loads successfully
with zero entries. Ask what actually happened:

```c
ci18n_load_language("en", path);

const ci18n_load_stats_t *st = ci18n_last_load_stats();
if (st->lines_malformed) {
    fprintf(stderr, "%s: %u bad lines, first at line %u\n", path,
            (unsigned)st->lines_malformed,
            (unsigned)st->first_malformed_line);
}
```

The stats also count keys, values and lines that were truncated to fit the
`CI18N_MAX_*` limits. A load that dropped or cut anything leaves
`ci18n_last_error()` at `CI18N_ERR_PARSE`, so a single check is enough if you
do not need the detail.

One note on the defaults: `CI18N_MAX_LINE_LENGTH` and
`CI18N_MAX_VALUE_LENGTH` are both 4096, so an over-long value is cut by the
line limit first. Its tail then arrives as another line with no separator and
is counted as malformed. Raise `CI18N_MAX_LINE_LENGTH` above
`CI18N_MAX_VALUE_LENGTH` if you want long values handled cleanly.

### Language codes

Codes must fit `CI18N_MAX_CODE_LENGTH`, 32 bytes including the terminator by
default. A longer code is rejected with `CI18N_ERR_CODE_TOO_LONG` rather than
truncated, because truncating used to mean the value could be written and
never read back.

### Plurals

A key-value table cannot translate Russian, which needs three forms where
English needs two. Plural forms are ordinary keys with the CLDR category in
brackets:

```ini
# en
files[one]=%d file
files[other]=%d files

# ru
files[one]=%d файл
files[few]=%d файла
files[many]=%d файлов
```

Then ask by count:

```c
printf(ci18n_plural_or_key("files", n), n);
```

```
en:   1 file    2 files    5 files    11 files    21 files
ru:   1 файл    2 файла    5 файлов   11 файлов   21 файл
```

Note 11 and 21: a rule that just checks for 1 gets Russian wrong, which is
why the category comes from CLDR rather than from the caller. Lookup tries
`key[category]`, then `key[other]`, then plain `key`, so a translation only
has to be as detailed as it needs to be.

Rules are known for English-like languages, French and Portuguese, Russian,
Ukrainian and Belarusian, Polish, Czech and Slovak, Croatian and Serbian,
Arabic, Lithuanian, Latvian, Slovenian, Irish, Romanian, and languages with
no plural distinction such as Japanese, Chinese and Korean. An unknown
language is treated as English-like. Only integer counts are considered.

### Interpolation

Values go into translations by name, not by position, because the order they
appear in is the translation's business:

```ini
# en
greeting=Hello, {name}! You have {count} messages.

# ru
greeting=Привет, {name}! У вас {count} сообщений.
```

```c
char text[256];
ci18n_format(text, sizeof(text), "greeting", "name", user, "count", "3", NULL);
```

Together with plurals, which is where it earns its keep:

```ini
inbox[one]={name}, у вас {count} новое сообщение
inbox[few]={name}, у вас {count} новых сообщения
inbox[many]={name}, у вас {count} новых сообщений
```

```c
ci18n_format_plural(text, sizeof(text), "inbox", n, "name", user, NULL);
```

```
Илья, у вас 1 новое сообщение
Илья, у вас 3 новых сообщения
Илья, у вас 5 новых сообщений
```

`{count}` is filled from the count itself, so it needs no pair. Pass one
anyway to override it, which is how you render "99+" while still selecting
the right form.

Both functions follow `snprintf()`: at most `capacity - 1` bytes are written,
the result is always terminated, and the return value is the length the whole
result would have had, so a return of `capacity` or more means truncation.
`ci18n_format(NULL, 0, ...)` measures without writing.

Two deliberate choices worth knowing:

- **Values are strings, never a format string.** A translation file is data,
  often written by someone who is not the programmer. Passing it to
  `printf()` as a format would make every translator a potential attacker.
- **An unmatched placeholder stays as written.** `{nmae}` comes out as
  `{nmae}`, so a typo is visible rather than a silent hole.

Write `{{` and `}}` for literal braces.

### Locale detection

```c
char locale[CI18N_MAX_CODE_LENGTH];
ci18n_detect_locale(locale, sizeof(locale));   /* "ru-RU" */

ci18n_set_current_best(NULL);                  /* use what the system says */
ci18n_set_current_best("ru-RU");               /* or a locale you choose */
```

`ci18n_detect_locale()` reads `LC_ALL`, `LC_MESSAGES` and `LANG`, and on
Windows asks the system for the user's default locale. It normalises the
result, so `ru_RU.UTF-8` comes back as `ru-RU`, and `C` or `POSIX` report
nothing because they name no language.

`ci18n_set_current_best()` drops subtags until something matches: `ru-RU`,
then `ru`. That means you can ship one plain `ru` file and still honour a
user asking for Russian as spoken in Russia. If nothing matches, the current
language is left as it was.

Define `CI18N_NO_PLATFORM_LOCALE` to keep `windows.h` out of your build and
rely on the environment variables alone.

### Threads

`CI18N_THREAD_LOCAL_CONTEXT` gives every thread its own context. Read that
literally: it is isolation, not shared thread safety. Each thread starts empty
and calls `ci18n_init()` and the loaders itself, and a language loaded on one
thread is invisible to the others. That suits a worker rendering in one user's
locale.

What it does not give you is "load once, read from many threads". Without the
macro the context is a single global with no locking, so a concurrent
`ci18n_set()` or `ci18n_load_*()` against a concurrent `ci18n_get()` is a data
race. If your threads only read, and every load finished before you spawned
them, the plain global is already safe.

`CI18N_THREAD_SAFE` is the old name for this macro. It still works and still
means exactly the same thing, but it emits a deprecation note.

### Pointer lifetime

Every `const char *` the API returns points into library storage, so it stays
valid only until the next call that mutates that language:

```c
const char *greeting = ci18n_get("greeting");
ci18n_load_language("en", "extra.txt");   /* may realloc */
puts(greeting);                           /* dangling */
```

`ci18n_set()`, `ci18n_remove()` and the loaders may reallocate the entry array.
`ci18n_clear()`, `ci18n_free()` and `ci18n_set_current()` invalidate pointers
outright. Read a translation right before you use it, which is cheap, or copy
it if you need to hold on to it.

### Version check

```c
#if CI18N_VERSION < CI18N_VERSION_NUMBER(2, 3, 0)
#error "ci18n 2.3.0 or newer is required"
#endif

printf("ci18n %s\n", CI18N_VERSION_STRING);
```

## API

| Function                                 | Description           |
| ---------------------------------------- | --------------------- |
| `ci18n_init()`                           | Initialize the system |
| `ci18n_free()`                           | Free resources        |
| `ci18n_load_language(code, path)`        | Load from file        |
| `ci18n_load_from_buffer(code, buf, len)` | Load from buffer      |
| `ci18n_set_current(code)`                | Set current language  |
| `ci18n_set_fallback(code)`               | Set fallback language |
| `ci18n_get(key)`                         | Get translation       |
| `ci18n_get_or_key(key)`                  | Translation or key    |
| `ci18n_has(key)`                         | Check if key exists   |
| `ci18n_set(lang, key, value)`            | Add translation       |
| `ci18n_remove(lang, key)`                | Remove translation    |
| `ci18n_clear(lang)`                      | Clear language        |
| `ci18n_count(lang)`                      | Entry count           |
| `ci18n_get_languages(out, cap)`          | List of languages     |
| `ci18n_last_error()`                     | Why the last call failed |
| `ci18n_error_string(err)`                | Error code as text    |
| `ci18n_last_load_stats()`                | What the last load did |
| `ci18n_plural(key, n)`                   | Plural form for a count |
| `ci18n_plural_or_key(key, n)`            | Plural form, or the key |
| `ci18n_plural_category(lang, n)`         | CLDR category for a count |
| `ci18n_plural_category_name(cat)`        | Category as text      |
| `ci18n_format(out, cap, key, ...)`       | Fill named placeholders |
| `ci18n_format_plural(out, cap, key, n, ...)` | Plural form, filled |
| `ci18n_detect_locale(out, cap)`          | Locale from the system |
| `ci18n_set_current_best(locale)`         | Best match for a locale |

## Installing

Three ways in, in rough order of how little they ask of you.

### Vendor the header

Copy [include/ci18n.h](include/ci18n.h) into your project. There is nothing
else to it: no library to link, no build step. This is the point of a single
header, and it is a perfectly respectable choice.

### CMake

As a dependency fetched at configure time:

```cmake
include(FetchContent)
FetchContent_Declare(ci18n
  GIT_REPOSITORY https://github.com/ilyabrin/ci18n.git
  GIT_TAG v2.3.0)
FetchContent_MakeAvailable(ci18n)

target_link_libraries(your_target PRIVATE ci18n::ci18n)
```

Or against an installed copy:

```bash
cmake -B build
cmake --build build
cmake --install build --prefix /usr/local
```

```cmake
find_package(ci18n 2.0 REQUIRED)
target_link_libraries(your_target PRIVATE ci18n::ci18n)
```

`ci18n::ci18n` is an INTERFACE target: it carries the include path and requires
C99, and links nothing. As a subproject it builds no tests and no example, and
adds no install rules.

### make

```bash
make install                       # into /usr/local
make install PREFIX=$HOME/.local
make install DESTDIR=/tmp/stage    # staging root for a package build
make uninstall
```

This installs the header and a pkg-config file, which is the same file the
CMake route installs:

```bash
cc $(pkg-config --cflags ci18n) -o app app.c
```

## Building

```bash
make              # build the example, then build and run the tests
make test         # tests only
make test-threads # thread-local context tests, needs pthreads
make clean
```

Or without make:

```bash
gcc -Wall -Wextra -std=c99 -I./include -o example examples/example.c
./example
```

Run both from the repository root so the relative paths in
`translations/` resolve.

## Contributing

Bug reports and pull requests are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md) for how to build, test and submit a change,
and [SECURITY.md](SECURITY.md) for reporting a vulnerability. Release history is
in [CHANGELOG.md](CHANGELOG.md).

This English README is the canonical one. The Russian translation is updated on
a best-effort basis, so when the two disagree, this file wins.

## License

MIT, see [LICENSE](LICENSE).
