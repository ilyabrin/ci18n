# Getting started

From nothing to a translated string, then the handful of things every
program runs into: errors, language codes, how long a returned pointer
lives, and which language to pick at startup.

- [Include](#include)
- [Load and pick a language](#load-and-pick-a-language)
- [Look things up](#look-things-up)
- [Clean up](#clean-up)
- [When something goes wrong](#when-something-goes-wrong)
- [Language codes](#language-codes)
- [Picking the user's language](#picking-the-users-language)
- [Pointer lifetime](#pointer-lifetime)
- [Version check](#version-check)

## Include

In **one** .c file of your project:

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

In every other file that uses it:

```c
#include "ci18n.h"
```

That is the whole setup. [Installing](../README.md#install) covers CMake,
pkg-config and `make install` if you would rather not copy the header.

## Load and pick a language

```c
ci18n_init();
ci18n_load_language("en", "translations/en.txt");
ci18n_load_language("ru", "translations/ru.txt");
ci18n_set_current("ru");
ci18n_set_fallback("en");
```

A translation file is one `key=value` per line; see
[Translations](translations.md). No file system? Load from memory with
`ci18n_load_from_buffer(code, text, length)`.

The fallback language answers every key the current one lacks, so a half
translated language still shows something readable.

## Look things up

```c
printf("%s\n", ci18n_get("welcome_message"));
// or with fallback to key:
printf("%s\n", ci18n_get_or_key("missing_key"));
```

`ci18n_get()` returns `NULL` for a key nobody has. `ci18n_get_or_key()`
returns the key itself instead, which is usually what you want in UI code:
a missing translation shows up as `welcome_message` rather than a crash.

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

## Clean up

```c
ci18n_free();
```

## When something goes wrong

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
`CI18N_MAX_VALUE_LENGTH` if you want long values handled cleanly. The limits
are listed in [Embedded and size](embedded-and-size.md#limits).

## Language codes

Codes must fit `CI18N_MAX_CODE_LENGTH`, 32 bytes including the terminator by
default. A longer code is rejected with `CI18N_ERR_CODE_TOO_LONG` rather than
truncated, because truncating used to mean the value could be written and
never read back.

## Picking the user's language

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

## Pointer lifetime

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
it with `ci18n_get_copy()` if you need to hold on to it. A string from a
[compiled language](compiled-catalogs.md) is the exception: it lives as long
as the program. With threads, always
copy; see [Catalogues and threads](catalogs-and-threads.md#the-shared-mode).

## Version check

```c
#if CI18N_VERSION < CI18N_VERSION_NUMBER(2, 6, 0)
#error "ci18n 2.6.0 or newer is required"
#endif

printf("ci18n %s\n", CI18N_VERSION_STRING);
```
