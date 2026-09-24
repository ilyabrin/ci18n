# ci18n [![CI](https://github.com/ilyabrin/ci18n/actions/workflows/ci.yml/badge.svg)](https://github.com/ilyabrin/ci18n/actions)

Internationalization for C in one header: translations, plurals for 71
languages, formatting, and right-to-left support, with no dependencies.

**Other languages:** [Русский](README.ru.md)

## In 30 seconds

`ru.txt`:

```ini
greeting=Привет, {name}!
files[one]={count} файл
files[few]={count} файла
files[many]={count} файлов
```

`main.c`:

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"

int main(void)
{
    char text[128];

    ci18n_init();
    ci18n_load_language("ru", "ru.txt");
    ci18n_set_current("ru");

    ci18n_format(text, sizeof(text), "greeting", "name", "Анна", NULL);
    puts(text);                                          /* Привет, Анна! */

    ci18n_format_plural(text, sizeof(text), "files", 3, NULL);
    puts(text);                                          /* 3 файла */

    ci18n_free();
}
```

```bash
cc -std=c99 -Iinclude main.c -o hello && ./hello
```

Next: [Getting started](docs/getting-started.md).

## Install

Copy [include/ci18n.h](include/ci18n.h) into your project. There is nothing
else to it: no library to link, no build step.

Or with CMake:

```cmake
include(FetchContent)
FetchContent_Declare(ci18n
  GIT_REPOSITORY https://github.com/ilyabrin/ci18n.git
  GIT_TAG v2.18.0)
FetchContent_MakeAvailable(ci18n)

target_link_libraries(your_target PRIVATE ci18n::ci18n)
```

Or install it system-wide, which gives you `find_package(ci18n)` and a
pkg-config file:

```bash
cmake -B build && cmake --build build && cmake --install build --prefix /usr/local
make install PREFIX=$HOME/.local           # the same, without CMake
cc $(pkg-config --cflags ci18n) -o app app.c
```

`ci18n::ci18n` is an INTERFACE target: it carries the include path and
requires C99, and links nothing. As a subproject it builds no tests and no
example, and adds no install rules. `make install DESTDIR=...` stages a
package build, and `make uninstall` undoes it.

With vcpkg, add the [ci18n registry](https://github.com/ilyabrin/vcpkg-registry)
to your `vcpkg-configuration.json`, and then `find_package(ci18n)` works as
above. For Arduino and PlatformIO it is a library like any other; see
[Arduino and AVR](docs/embedded-and-size.md#arduino-and-avr).

## What it does

| | |
| --- | --- |
| **Translations** | `key=value` files or buffers, merged per language, with a fallback language for what is missing. [Translations](docs/translations.md) |
| **Plurals and ordinals** | 1 file, 3 файла, 22nd: CLDR rules for 71 languages, checked against CLDR by the tests. [Plurals](docs/translations.md#plurals) |
| **Formatting** | Named placeholders, your own formatters for dates, and numbers in each language's style. [Formatting](docs/formatting.md) |
| **Right to left** | Text direction per language, and bidi isolation for mixed sentences. [Unicode and text direction](docs/unicode-and-direction.md) |
| **UTF-8** | Strict validation, character counts, truncation that never splits a character. [UTF-8 helpers](docs/unicode-and-direction.md#utf-8-helpers) |
| **gettext** | Load `.mo` files directly, or convert `.po` files once. [Coming from gettext](docs/from-gettext.md) |
| **Libraries and threads** | Independent catalogues, a thread-local mode, and a shared mode whose reads scale with cores. [Catalogues and threads](docs/catalogs-and-threads.md) |
| **Compiled in** | Translations as constant data: no heap, no loading, and `CI18N_KEY(greeting)` makes a mistyped key a compile error. [Compiled catalogues](docs/compiled-catalogs.md) |
| **Small** | 28 KB of code at `-Os` with everything in, 15 KB with `CI18N_MINIMAL`, 7.6 KB on a Cortex-M4, and it runs on an Arduino Uno. [Embedded and size](docs/embedded-and-size.md) |
| **Checked** | Warnings as errors, sanitizers and fuzzing, on Linux, macOS, Windows, the BSDs, iOS, Android, WebAssembly, bare-metal ARM and RISC-V, and 8-bit AVR. [Platforms](docs/platforms.md) |

A lookup takes about 28 ns, and glibc's `gettext` about 140 ns on the same
machine. [Performance](docs/performance.md)

## What it does not do

No sorting, time zones, dates, currencies, normalization or word breaking:
those need ICU's data, and ci18n will call your function for dates and
currencies instead. [Scope](docs/scope.md) explains the line and when to pick
gettext or ICU.

## Examples

Three programs of the kind people build with this, each with a short README.
`make examples` builds all three and checks their output.

| Example | What it is | Shows |
| --- | --- | --- |
| [cli_sync](examples/cli_sync/) | A CI check that finds missing keys, plural forms and placeholders | Catalogues, `ci18n_foreach`, load stats, fallback |
| [server](examples/server/) | Worker threads serving requests in the language each one asks for | Shared mode, `Accept-Language`, formatters, reload under readers |
| [embedded_ui](examples/embedded_ui/) | A 20x4 display in English, Russian and Arabic | Small limits, packs from outside, fitting UTF-8, right-to-left |

[examples/example.c](examples/example.c) is the short tour of the basics.

## Documentation

All of it is in [docs/](docs/README.md): a getting-started page, one page per
feature, the [API reference](docs/api.md), and the numbers. Every function is
also documented next to its declaration in the header.

## Building and testing

```bash
make              # build the example, then build and run the tests
make test         # the unit tests only
make examples     # build the three examples and check their output
make test-threads test-shared   # the threading modes, needs pthreads
make bench        # timings, see bench/README.md
make clean
```

Run them from the repository root, so the relative paths in `translations/`
resolve. [CONTRIBUTING.md](CONTRIBUTING.md) lists every target, including the
fuzzer and the gettext round trips.

## Contributing

Bug reports and pull requests are welcome. See
[CONTRIBUTING.md](CONTRIBUTING.md) for how to build, test and submit a change,
[SECURITY.md](SECURITY.md) for reporting a vulnerability, and
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) for how people are expected to behave.
Release history is in [CHANGELOG.md](CHANGELOG.md).

The Russian documentation, [README.ru.md](README.ru.md) and
[docs/ru/](docs/ru/README.md), is a translation of this one, and CI checks
that the two match in everything a program can compare: sections, code
examples, tables, links and numbers. Where the prose still disagrees, the
English is right.

## License

MIT, see [LICENSE](LICENSE).
