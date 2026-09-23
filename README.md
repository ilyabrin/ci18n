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

## What this does not do

The boundary is deliberate. Knowing where it sits should save you an
afternoon.

### Not in scope, and not planned

| Not in scope | Use instead |
| --- | --- |
| Sorting strings in locale order | ICU |
| Unicode normalization, NFC and NFD | ICU |
| Time zones | ICU, or your platform |
| Date, time and calendar formatting | ICU, or `strftime`, through a formatter |
| Currency formatting | ICU, through a formatter |
| Percent, compact "1.2K" and native digits | ICU, through a formatter |
| Transliteration | ICU |
| Word and line breaking for languages written without spaces | ICU |
| Case mapping beyond ASCII, such as Turkish dotless i | ICU |
| Extracting translatable strings from source code | gettext's `xgettext` |

The two marked "through a formatter" are worth a word: the library will
not render a date or a currency, but it will call your function to do it,
in the place the translator chose. See "Formatters" below.

These are a decision rather than a backlog. Almost all of ICU's tens of
megabytes are CLDR *data*, not code: patterns for some six hundred locales,
collation tables, the Unicode character database, normalization tables, the
timezone database, dictionaries for languages written without spaces.
Shipping that would end both the single header and the kilobyte of heap
that a catalogue costs today.

The decisive argument is maintenance rather than size. CLDR releases twice a
year, and the timezone database changes about ten times a year because
countries change their minds about daylight saving. A library with one
maintainer shipping stale timezone or currency data would be confidently
wrong, which is worse than not offering it at all.

### Arrived recently

Text direction arrived in 2.7.0, UTF-8 helpers and pluggable formatters in
2.9.0, ordinals in 2.10.0, bidi isolation and `.mo` loading in 2.13.0, and
per-language number separators in 2.14.0. Each has a section of its own below.

### Which one to pick

- **ci18n**, if you have up to a few hundred strings, one or two people
  translating them, and you need strings rather than date and number
  formatting. Or if size and ease of vendoring matter to you.
- **gettext**, if you have many strings and separate translators, and you want
  `xgettext` so that nobody maintains a list of keys by hand.
- **ICU**, if your interface has dates, numbers, currencies, sorting, or
  bidirectional text in earnest.

Nothing here is an argument that ci18n is better. It is smaller, and that is a
different claim.

## Features

- ✅ Single header file
- ✅ Pure C, C99 and newer
- ✅ No external dependencies
- ✅ Multiple language support
- ✅ Load from files and buffers
- ✅ Fallback language
- ✅ Thread-local context (optional)
- ✅ UTF-8 aware: strict validation, and truncation that keeps characters whole

Small enough to mean it. Three translation files of about ten keys each cost
1600 bytes of heap, and a lookup among a thousand keys takes about 25 ns.
Nothing is allocated until you store something.

## Platforms

Every push builds with warnings as errors and runs the unit tests on each of
these. "Build only" means there is nothing in CI to run the result on.

| Platform | Compilers | Tests run |
| --- | --- | --- |
| Linux x86-64, ARM64, 32-bit x86 | gcc, clang, gcc 9, clang 12 | yes, plus sanitizers and fuzzing |
| Linux on s390x (big-endian), ARMv7, RISC-V 64 | gcc, emulated | yes |
| Linux with musl (Alpine) | gcc | yes |
| macOS on Apple silicon and Intel | Apple clang | yes |
| Windows x64 and ARM64 | MSVC, clang-cl, MinGW gcc | yes |
| FreeBSD, OpenBSD, NetBSD | system cc | yes |
| iOS | Apple clang | yes, on the simulator; device build only |
| Android arm64, armv7, x86-64 | NDK clang | build only |
| WebAssembly | Emscripten | yes, under Node |
| Cortex-M0+, M3, M4, M7 | arm-none-eabi-gcc, newlib | yes on M3, under QEMU |
| RISC-V 32, the ESP32-C3 class | riscv gcc, picolibc | yes, under QEMU |

The smallest target is a 32-bit microcontroller with 64 KB of RAM. 8-bit AVR
boards such as the Arduino Uno are not supported: the library keeps strings
on the heap and needs `fopen` for its file loaders, and an Uno has 2 KB of RAM.
See [.github/workflows/platforms.yml](.github/workflows/platforms.yml).

## Quick Start

The smallest program that works needs one header and one file per language:

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"

int main(void)
{
    ci18n_init();
    ci18n_load_language("ru", "ru.txt");   /* greeting=Привет */
    ci18n_set_current("ru");
    puts(ci18n_get_or_key("greeting"));
    ci18n_free();
}
```

Everything else below is optional, and costs nothing until you call it.

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

## Examples

Three programs of the kind people build with this, each with a short README.
`make examples` builds all three and checks their output.

| Example | What it is | Shows |
| --- | --- | --- |
| [cli_sync](examples/cli_sync/) | A CI check that finds missing keys, plural forms and placeholders | Catalogues, `ci18n_foreach`, load stats, fallback |
| [server](examples/server/) | Worker threads serving requests in the language each one asks for | Shared mode, `Accept-Language`, formatters, reload under readers |
| [embedded_ui](examples/embedded_ui/) | A 20x4 display in English, Russian and Arabic | Small limits, packs from outside, fitting UTF-8, right-to-left |

[examples/example.c](examples/example.c) is the short tour of the basics.

## Translation File Format

```ini
# This is a comment
greeting=Hello!
farewell=Goodbye!
error=An error occurred
```

## Coming from gettext

Load the `.mo` files you already build, no libintl and no conversion step:

```c
ci18n_load_mo("ru", "locale/ru/LC_MESSAGES/app.mo");
ci18n_set_current("ru");
ci18n_get("Hello, world");     /* keys are the msgids */
ci18n_plural("%d file", n);    /* msgid_plural works too */
```

A `msgctxt` becomes a prefix, `menu.Open`, and plural forms land on CLDR
categories by the header's `nplurals`. Both byte orders are read, and every
offset is checked before use, so a broken file is refused rather than read
past. `ci18n_load_mo_from_buffer` takes one already in memory. The loader is
about 2.5 KB of code; define `CI18N_NO_MO` to leave it out.

To leave gettext entirely, `tools/po2ci18n.py` converts a `.po` catalogue into a ci18n translation file:

```bash
tools/po2ci18n.py ru.po -o translations/ru.txt
tools/po2ci18n.py --keys=slug ru.po -o translations/ru.txt
```

It handles escapes and the wrapped continuation lines gettext writes, maps
`msgid_plural` and its indexed `msgstr[0]`, `msgstr[1]` onto CLDR category
names, turns `msgctxt` into a key prefix, and skips fuzzy, untranslated and
obsolete entries. `--keys=slug` turns English sentences into short
identifiers, which avoids `CI18N_MAX_KEY_LENGTH`.

There is deliberately no `.po` reader in the library. A full one is around 700
lines, 300 of them an evaluator for the C expression gettext puts in its
Plural-Forms header, which would double the parse surface of a library whose
point is one small header. Converting at build time evaluates that expression
on your machine rather than on the device.

`make test-po` runs the converter over a sample catalogue and loads the result
back, so it stays correct. `make test-mo` compiles the same sample with
`msgfmt`, in both byte orders, and checks that the `.mo` loader ends up with
exactly what the converter wrote.

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

### Leaving parts out

Everything is in by default. Each of these takes a feature out, its code and
its data, and stops declaring its functions, so a call to one is a compile
error rather than a surprise at run time:

| Macro | Removes | Saves at `-Os`, x86-64 / Cortex-M4 |
| --- | --- | --- |
| `CI18N_NO_FORMAT` | `ci18n_format` and relatives, formatters, bidi isolation | 4.9 / 1.8 KB, and 260 to 330 bytes per catalogue |
| `CI18N_NO_MO` | the gettext `.mo` loader | 2.7 / 1.3 KB |
| `CI18N_NO_ORDINALS` | `ci18n_ordinal` and relatives | 2.6 / 1.0 KB |
| `CI18N_NO_NUMBERS` | `ci18n_format_number` and `{n:number}` | 2.2 / 1.4 KB |
| `CI18N_NO_FILES` | every loader that opens a file | 1.2 / 0.4 KB |
| `CI18N_NO_LOCALE` | `ci18n_detect_locale`, `ci18n_set_current_best` | 0.7 / 0.4 KB |
| `CI18N_MINIMAL` | all of the above | 13 / 6.0 KB, about half |

What stays in every build: loading from buffers, lookup, plurals, the
fallback language, catalogues, text direction, the UTF-8 helpers and the
diagnostics. CI builds and tests each of these on its own, and all together.

**When this matters, and when the linker already does it.** The savings
above are for the whole implementation, which is what you get from gcc and
clang by default and from any shared library, since every public function is
kept. With `-ffunction-sections -fdata-sections -Wl,--gc-sections`, the
default in ESP-IDF, Zephyr, STM32Cube and the Arduino cores, the linker drops
what you never call on its own. There [tests/minimal.c](tests/minimal.c), a
lookup and a plural, takes 7.1 KB of library code on a Cortex-M4 with
everything in and 6.2 KB with `CI18N_MINIMAL`, and the macros save what the
linker cannot see through: the ordinal rules the plural lookup is wired to,
and 260 bytes of RAM per catalogue for the formatter table.

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
has to be as detailed as it needs to be. All three come from one language:
the current one first, and only when it has none of them, the fallback, with
the fallback's own rules picking the form.

Rules are known for 71 languages, grouped the way CLDR groups them:
English-like, French and Portuguese, Russian, Ukrainian and Belarusian,
Polish, Czech and Slovak, Croatian and Serbian, Arabic, Hebrew with its dual,
Lithuanian, Latvian, Slovenian, Irish, Romanian, and languages with no plural
distinction such as Japanese, Chinese and Korean. French, Portuguese,
Spanish, Italian and Catalan also have a "many" form for a whole number of
millions, "1 000 000 de fichiers", which a translation can skip since lookup
falls back to `[other]`. An unknown language is treated as English-like. Only
integer counts are considered.

Every supported language is checked against CLDR 48 by the unit tests, using
the sample numbers CLDR publishes next to each rule. See
[tools/cldr_samples.py](tools/cldr_samples.py).

### Ordinals

First, second, third is a different question from one, two, three, with its
own rules. English has four ordinal forms, and 11, 12 and 13 take "th" despite
ending in 1, 2 and 3. Russian, German, Spanish and most others have only one,
because their ordinal is a word that agrees with its noun rather than a
suffix.

Ordinal keys use the same brackets as plural keys:

```ini
place[one]={count}st place
place[two]={count}nd place
place[few]={count}rd place
place[other]={count}th place
```

```c
ci18n_format_ordinal(out, sizeof(out), "place", 22, NULL);  /* 22nd place */
ci18n_format_ordinal(out, sizeof(out), "place", 12, NULL);  /* 12th place */
```

A German translation writes only `place[other]={count}. Platz`, and every
count uses it. Keep ordinal and cardinal forms under different keys: `files[one]`
and `place[one]` mean different things.

`ci18n_ordinal(key, n)` returns the form without filling it, and
`ci18n_ordinal_category(lang, n)` returns just the category. An unknown
language has no ordinal forms, since "other" is the one every translation has.

### Text direction

Arabic, Hebrew and Persian are written right to left, and an interface has to
know which way round to put things. The direction is a property of the
language, so the library answers that; the layout is yours.

```c
ci18n_set_current("ar");

ci18n_current_direction();                        /* CI18N_DIR_RTL */
ci18n_direction_name(ci18n_current_direction());  /* "rtl" */
```

The names are the ones HTML and CSS use, so they drop straight in:

```c
printf("<html lang=\"%s\" dir=\"%s\">\n",
       ci18n_get_current(),
       ci18n_direction_name(ci18n_current_direction()));
```

`ci18n_direction(code)` answers for any code without touching the current
language. A script subtag decides on its own, because direction belongs to
the script rather than the language:

| Code | Direction | Why |
| --- | --- | --- |
| `ar`, `ar-EG`, `ar_EG.UTF-8` | rtl | Arabic, region and charset ignored |
| `he`, `fa`, `ur`, `ps`, `dv`, `yi`, `ckb` | rtl | default script is right to left |
| `ku`, `az`, `pa` | ltr | Latin or Gurmukhi unless told otherwise |
| `az-Arab`, `pa-Arab`, `ku-Arab` | rtl | the script subtag overrides |
| `ar-Latn` | ltr | romanized Arabic reads left to right |
| `xx`, `""`, `NULL` | ltr | unknown, and left to right is the safe guess |

An unknown language is left to right. That is the commoner answer and the
safer one: a left-to-right interface shown right to left is broken in a way
nobody misses, while the reverse merely looks untranslated.

### Mixed-direction text

A value in the other direction reorders the sentence around it. An English
name inside Arabic drags the comma after it to the wrong side, and a Hebrew
name in an English sentence can swap places with the number beside it. The
fix is to isolate each value, so the renderer lays it out on its own:

```c
ci18n_set_bidi_isolation(true);
ci18n_format(out, sizeof(out), "inbox", "name", "Sam", NULL);
/* "{name}، لديك..." becomes FSI "Sam" PDI "، لديك..." */
```

Every value `ci18n_format` and its relatives fill in, `{count}` and
formatter output included, is wrapped in FSI and PDI, two invisible
characters that browsers, GTK, Qt, Android and iOS all honour. Text the
translator wrote is never wrapped. The setting is per catalogue and off by
default: turn it on for text people read, and leave it off for logs and
anything a program parses, since the marks are real bytes, 3 each.

| For | Use |
| --- | --- |
| A value placed without `ci18n_format` | `ci18n_bidi_isolate(out, cap, text)` |
| A line ending in a number or a symbol | append `ci18n_bidi_mark(dir)`, LRM or RLM |
| The raw characters | `CI18N_FSI`, `CI18N_PDI`, `CI18N_LRM`, `CI18N_RLM` |

This marks text for a renderer that runs the Unicode bidirectional
algorithm. It does not run the algorithm itself, so a display controller that
draws bytes left to right as they come still needs a bidi step of its own.

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

### UTF-8

The library is byte-transparent: it stores and returns what you give it, so
UTF-8 passes through untouched. These helpers exist for the two places where
bytes are not enough.

Counting characters is not counting bytes:

```c
strlen("Привет");            /* 12 */
ci18n_utf8_length("Привет");  /* 6  */
```

And cutting a string to fit a buffer can land in the middle of a character,
which produces bytes no decoder accepts. `ci18n_utf8_truncate` cuts only at a
boundary:

```c
char label[16];

ci18n_get_copy("title", label, sizeof(label));
ci18n_utf8_truncate(label, sizeof(label) - 1);
```

The budget is the text length, terminator not counted, so a `char[16]` takes
`sizeof(buffer) - 1`. The result can come out up to three bytes short of the
budget, because the cut moves back rather than splitting a character. Nothing
is appended, so if you want an ellipsis there is room for one.

`ci18n_format` and `ci18n_format_plural` already do this for you: a truncated
result is always valid UTF-8. The reported length is still the full length, so
sizing a buffer works as before.

```c
char out[8];

ci18n_set("ru", "greeting", "Привет");
ci18n_format(out, sizeof(out), "greeting", NULL);  /* returns 12 */
/* out is "При", 6 bytes, not 7 bytes ending in half a character */
```

Validation is strict, in the sense the Unicode standard requires:

```c
ci18n_utf8_valid(text);
```

It rejects what is not UTF-8, rather than only what fails to decode: a
character written in more bytes than it needs, a surrogate half from U+D800
to U+DFFF, anything above U+10FFFF, a continuation byte with no lead byte,
and a sequence the string ends in the middle of. Lenient decoders are how a
check on one representation gets bypassed with another.

To walk a string a character at a time:

```c
for (const char *p = text; *p; p += ci18n_utf8_sequence_length(p))
{
    /* p points at one whole character */
}
```

That never stalls: an invalid byte reports 1, stepping past the problem
instead of looping forever on it.

One honest limit. "Character" here means one Unicode codepoint, which is the
useful answer for a length limit but still not the number of things a reader
would count. An accent written as a separate combining mark is its own
codepoint, and a single emoji can be several. Counting those needs grapheme
clusters, which needs the Unicode character database, which is exactly the
kind of data this library does not ship.

### Formatters

A date in a sentence is a translation problem twice over. Where it goes is the
translator's business, which named placeholders already solve. How it reads is
the locale's business, which this library will never know, because knowing it
means shipping CLDR.

So the translation names a formatter and your code supplies it:

```ini
invoice=Issued {created:date,long}, due {due:date,short}
```

```c
static size_t format_date(char *out, size_t capacity, const char *value,
                          const char *arg, void *user_data)
{
    (void)user_data;
    return my_render_date(out, capacity, value, arg);  /* your date code */
}

ci18n_set_formatter("date", format_date, NULL);

ci18n_format(text, sizeof(text), "invoice",
             "created", "2026-09-23",
             "due", "2026-10-07", NULL);
```

The library does the parsing, the lookup and the buffer arithmetic. You do the
rendering, with whatever you already use for dates. A translator can move the
placeholder, ask for a different form, or drop it, without touching your code.

The syntax is `{name:formatter}` or `{name:formatter,argument}`. Everything
after the first comma is the argument, verbatim, so a formatter can define its
own syntax there:

```ini
price=Total: {amount:currency,USD}
when=Updated {ts:relative}
pad=[{label:pad,12,right}]
```

Your function follows `snprintf`: write at most `capacity - 1` bytes,
terminate whenever capacity is non-zero, and return the length the whole
result would have had. The library calls with `out = NULL` and `capacity = 0`
to measure, so handle that without writing. `value` and `arg` are never NULL;
`arg` is `""` when the translation gave none. `user_data` is handed back
untouched, so it can carry your locale object.

A placeholder naming a formatter nobody registered is left visible and
`ci18n_last_error()` reports `CI18N_ERR_UNKNOWN_FORMATTER`. The rest of the
sentence still renders, because a missing formatter is a reason to see a
defect, not to lose the text around it.

```c
ci18n_format(out, sizeof(out), "invoice", "created", "2026-09-23", NULL);
/* out is "Issued {created:date,long}, due ..." and the error says why */
```

Formatters belong to a catalogue and are not inherited from the default one. A
library using its own catalogue registers its own, which is the point of
having catalogues, and it also means formatting never has to hold two locks at
once. Use `ci18n_set_formatter_in` and `ci18n_format_in` for that.

In the shared threading mode a formatter runs while the catalogue's read lock
is held. So it must not call back into the library, and it has to be safe to
run on several threads at once.

### Numbers

The same number is written four ways across four languages:

| Language | `1234567.5` | `{n:number,2}` of `19.999` |
| --- | --- | --- |
| en | 1,234,567.5 | 20.00 |
| de | 1.234.567,5 | 20,00 |
| ru | 1 234 567,5 | 20,00 |
| hi | 12,34,567.5 | 20.00 |

`number` is a built-in formatter, so a translation just names it:

```ini
total=Total: {n:number}
price=Price: {n:number,2} USD
files[other]={count:number} files
```

```c
ci18n_format(out, sizeof(out), "total", "n", "1234567.5", NULL);
ci18n_format_number(out, sizeof(out), "de", "1234567.5", -1);  /* directly */
```

- **The value is a string**, such as `"-1234.5"`, so a double cannot round
  it on the way. Print yours with `%ld` or `%.2f` first.
- **The argument is the fraction digits.** None keeps what the value has;
  `2` rounds or pads to two, half to even, as CLDR does.
- **Per language, from CLDR**: the decimal and group separators, the minus
  sign, whether 1234 is grouped at all (Spanish and Polish wait for 12 345),
  and the Indian 12,34,567. All 71 languages with plural rules are covered,
  in under 1 KB. [tools/cldr_numbers.py](tools/cldr_numbers.py) generates the
  table and checks every language against CLDR.
- **The current language decides.** An unknown one is written the English
  way, as it gets English plurals.
- **Anything else passes through.** `1e9` or `12,5` comes out as it went in,
  and a formatter you register as `number` replaces the built-in one.

Digits stay 0 to 9 everywhere: Arabic, Persian, Bengali and a few others
have native digits of their own, and CLDR lists Latin ones for every
language as well, which is what this writes.

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

### Catalogues

Everything above works on one catalogue the library owns. Convenient for a
program, wrong for a library: if ci18n is used inside a reusable component,
the component and the application that linked it share one current language,
and whichever called `ci18n_set_current()` last wins.

So every function has an `_in` variant taking a catalogue explicitly, and the
plain names are those variants applied to a default one:

```c
ci18n_t *ui   = ci18n_create();
ci18n_t *logs = ci18n_create();

ci18n_load_language_in(ui, "ru", "ru.txt");
ci18n_set_current_in(ui, "ru");

ci18n_load_language_in(logs, "en", "en.txt");
ci18n_set_current_in(logs, "en");

puts(ci18n_get_or_key_in(ui, "greeting"));     /* Russian */
puts(ci18n_get_or_key_in(logs, "greeting"));   /* English */

ci18n_destroy(ui);
ci18n_destroy(logs);
```

A catalogue carries its own languages, its own current and fallback selection,
and in the shared threading mode its own lock, so two of them never wait on
each other. `ci18n_default()` returns the one the plain functions use, so code
written against the `_in` functions can still reach it.

`ci18n_destroy(NULL)` is a no-op, so a failed create needs no special case.

### Threads

Three modes. Pick one before including the header; defining two is an error.

| Mode | What you get |
| --- | --- |
| nothing defined | One global context, no locking |
| `CI18N_THREAD_LOCAL_CONTEXT` | A separate context per thread |
| `CI18N_THREAD_SHARED` | One shared context behind a reader-writer lock |

**Default.** Safe when every load finished before the threads started and they
only read afterwards, which covers most programs. A concurrent `ci18n_set()`
or `ci18n_load_*()` against a concurrent `ci18n_get()` is a data race.

**`CI18N_THREAD_LOCAL_CONTEXT`.** Isolation, not sharing. Each thread calls
`ci18n_init()` and loads its own translations, and a language loaded on one
thread is invisible to the others. Suits a worker rendering in one user's
locale. `CI18N_THREAD_SAFE` is the old name for this one; it still works and
emits a deprecation note.

**`CI18N_THREAD_SHARED`.** One context, an rwlock around every entry point.
Readers do not block each other, a writer excludes everyone. This is the
"load once, read from many threads, reload occasionally" case.

```c
#define CI18N_THREAD_SHARED
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

Two things to know about the shared mode.

**Use `ci18n_get_copy()`, not `ci18n_get()`.** A lock cannot make a returned
pointer safe: the moment it is released, a writer may reallocate the storage
that pointer refers to. `ci18n_get_copy()` copies while the read lock is still
held.

```c
char text[128];
ci18n_get_copy("greeting", text, sizeof(text));
```

**Diagnostics are per-thread.** `ci18n_last_error()` and
`ci18n_last_load_stats()` describe the calling thread's last call, not the
context's. Sharing one slot would mean two threads overwriting each other.

On glibc this mode needs `-D_POSIX_C_SOURCE=200809L`, or `-std=gnu99` instead
of `-std=c99`, because strict ANSI mode hides the POSIX threading
declarations. The header says so with an `#error` if you forget.

**Reads scale with cores.** A catalogue holds 16 locks, each on its own
cache line, and every thread reads through its own one, so readers never
touch the same memory. With `make bench-threads`, one thread does 27 million
`ci18n_get` calls a second and eight threads 162 million between them, the
same as eight separate catalogues. The price is on the writing side: a
writer takes all 16 locks, so `ci18n_set()` and the loaders cost a little
more under this mode, which suits "load once, reload rarely".

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
#if CI18N_VERSION < CI18N_VERSION_NUMBER(2, 6, 0)
#error "ci18n 2.6.0 or newer is required"
#endif

printf("ci18n %s\n", CI18N_VERSION_STRING);
```

## API

| Function                                 | Description           |
| ---------------------------------------- | --------------------- |
| `ci18n_create()` / `ci18n_destroy(c)`    | Make or free a catalogue |
| `ci18n_default()`                        | The catalogue the plain names use |
| `ci18n_*_in(catalog, ...)`               | Any of the below, on that catalogue |
| `ci18n_init()`                           | Initialize the system |
| `ci18n_free()`                           | Free resources        |
| `ci18n_load_language(code, path)`        | Load from file        |
| `ci18n_load_from_buffer(code, buf, len)` | Load from buffer      |
| `ci18n_load_mo(code, path)`              | Load a gettext .mo file |
| `ci18n_load_mo_from_buffer(code, p, len)` | Load .mo bytes from memory |
| `ci18n_set_current(code)`                | Set current language  |
| `ci18n_set_fallback(code)`               | Set fallback language |
| `ci18n_get(key)`                         | Get translation       |
| `ci18n_get_or_key(key)`                  | Translation or key    |
| `ci18n_get_copy(key, out, cap)`          | Copy it into your buffer |
| `ci18n_has(key)`                         | Check if key exists   |
| `ci18n_set(lang, key, value)`            | Add translation       |
| `ci18n_remove(lang, key)`                | Remove translation    |
| `ci18n_clear(lang)`                      | Clear language        |
| `ci18n_remove_language(lang)`            | Unload it and free the slot |
| `ci18n_count(lang)`                      | Entry count           |
| `ci18n_foreach(lang, fn, ud)`            | Visit every entry     |
| `ci18n_get_languages(out, cap)`          | List of languages     |
| `ci18n_last_error()`                     | Why the last call failed |
| `ci18n_error_string(err)`                | Error code as text    |
| `ci18n_last_load_stats()`                | What the last load did |
| `ci18n_plural(key, n)`                   | Plural form for a count |
| `ci18n_plural_or_key(key, n)`            | Plural form, or the key |
| `ci18n_plural_category(lang, n)`         | CLDR category for a count |
| `ci18n_plural_category_name(cat)`        | Category as text      |
| `ci18n_ordinal(key, n)`                  | Ordinal form: 1st, 2nd, 3rd |
| `ci18n_ordinal_or_key(key, n)`           | Ordinal form, or the key |
| `ci18n_ordinal_category(lang, n)`        | CLDR ordinal category |
| `ci18n_format_ordinal(out, cap, key, n, ...)` | Ordinal form, filled |
| `ci18n_direction(code)`                  | Which way a language is written |
| `ci18n_direction_name(dir)`              | Direction as "ltr" or "rtl" |
| `ci18n_current_direction()`              | Direction of the current language |
| `ci18n_set_bidi_isolation(on)`           | Isolate filled-in values |
| `ci18n_bidi_isolate(out, cap, text)`     | Wrap one value in FSI ... PDI |
| `ci18n_bidi_mark(dir)`                   | LRM or RLM for a direction |
| `ci18n_utf8_valid(text)`                 | Is it well-formed UTF-8 |
| `ci18n_utf8_length(text)`                | Characters, not bytes |
| `ci18n_utf8_sequence_length(text)`       | Bytes in the character here |
| `ci18n_utf8_truncate(text, max)`         | Cut to fit, at a boundary |
| `ci18n_set_formatter(name, fn, ud)`      | Register a value renderer |
| `ci18n_remove_formatter(name)`           | Forget one |
| `ci18n_format_number(out, cap, lang, num, digits)` | A number the language's way |
| `ci18n_format_in(c, out, cap, key, ...)` | Fill placeholders, on that catalogue |
| `ci18n_format_plural_in(c, ...)`         | Plural form, filled, on that catalogue |
| `ci18n_format(out, cap, key, ...)`       | Fill named placeholders |
| `ci18n_format_plural(out, cap, key, n, ...)` | Plural form, filled |
| `ci18n_detect_locale(out, cap)`          | Locale from the system |
| `ci18n_set_current_best(locale)`         | Best match for a locale |

## Performance

Measured with `make bench` on an Intel Core i5-12400F, gcc 13 at `-O2`,
Linux, 1 000-key Russian catalogue with English as fallback. Your numbers
will differ; see [bench/README.md](bench/README.md) for the method and how to
run it.

| Operation | Time |
| --- | ---: |
| `ci18n_get`, key found | 24 ns |
| `ci18n_get`, found in the fallback language | 38 ns |
| `ci18n_get`, key missing | 41 ns |
| `ci18n_format_plural` | 91 ns |
| `ci18n_format`, two placeholders | 104 ns |
| `ci18n_format`, value through a formatter | 170 ns |
| Load 1 000 keys | 0.17 ms |
| Load 10 000 keys | 1.5 ms |

For reference, glibc's `gettext` on the same keys and machine takes 133 ns
for a found key and 679 ns for a missing one (`make bench-gettext`).

**Size.** Code and constant data of the implementation, measured with gcc:

| Build | x86-64 `-O2` | x86-64 `-Os` | Cortex-M4 `-Os` |
| --- | --- | --- | --- |
| Everything | 38 KB | 27 KB | 13 KB |
| `CI18N_MINIMAL` | 21 KB | 14 KB | 7 KB |

See "Leaving parts out" for what each module costs. The header itself is
208 KB of source, most of it comments, and is compiled in one file only.

**Memory.**

| What | Size |
| --- | --- |
| A catalogue, empty | 3.5 KB, no heap; 0.4 KB with `CI18N_MINIMAL` and 4 languages on a 32-bit target |
| The default catalogue | 3.5 KB of static storage |
| `CI18N_THREAD_LOCAL_CONTEXT` | 3.5 KB per thread that calls `ci18n_init()` |
| Loaded translations | about 1.4 times the file size |

A 1 000-key language from a 66 KB file takes 94 KB of heap, a 10 000-key
one from 664 KB takes 0.9 MB. A load gives back the spare room its buffers
grew into, which costs one copy and makes loading about 20% slower.

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
  GIT_TAG v2.16.0)
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
[SECURITY.md](SECURITY.md) for reporting a vulnerability, and
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for how people are expected to behave.
Release history is in [CHANGELOG.md](CHANGELOG.md).

This English README is the canonical one. The Russian translation is updated on
a best-effort basis, so when the two disagree, this file wins.

## License

MIT, see [LICENSE](LICENSE).
