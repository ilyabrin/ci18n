# Embedded and size

How big the library is, how to make it smaller, and how to fit it to a
device: limits, linkage and the smallest targets it runs on.

- [How big](#how-big)
- [Leaving parts out](#leaving-parts-out)
- [When the linker already does it](#when-the-linker-already-does-it)
- [Limits](#limits)
- [Memory](#memory)
- [Linkage](#linkage)
- [Small devices](#small-devices)

## How big

Code and constant data of the implementation, measured with gcc:

| Build | x86-64 `-O2` | x86-64 `-Os` | Cortex-M4 `-Os` |
| --- | --- | --- | --- |
| Everything | 40 KB | 28 KB | 13.6 KB |
| `CI18N_MINIMAL` | 22 KB | 15 KB | 7.6 KB |

The header itself is 208 KB of source, most of it comments, and is compiled
in one file only.

## Leaving parts out

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
| `CI18N_NO_COMPILED` | `ci18n_use_compiled`, [compiled catalogues](compiled-catalogs.md) | 0.8 / 0.4 KB |
| `CI18N_MINIMAL` | all of the above but `CI18N_NO_COMPILED` | 13 / 6.0 KB, about half |

```c
#define CI18N_MINIMAL
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

What stays in every build: loading from buffers, lookup, plurals, the
fallback language, catalogues, text direction, the UTF-8 helpers and the
diagnostics. `CI18N_MINIMAL` keeps compiled catalogues, which a small build
is the most likely to want; add `CI18N_NO_COMPILED` to drop them too. CI builds and tests each of these on its own, and all together.

## When the linker already does it

The savings above are for the whole implementation, which is what you get
from gcc and clang by default and from any shared library, since every
public function is kept. With
`-ffunction-sections -fdata-sections -Wl,--gc-sections`, the default in
ESP-IDF, Zephyr, STM32Cube and the Arduino cores, the linker drops what you
never call on its own. There [tests/minimal.c](../tests/minimal.c), a lookup
and a plural, takes 7.1 KB of library code on a Cortex-M4 with everything in
and 6.2 KB with `CI18N_MINIMAL`, and the macros save what the linker cannot
see through: the ordinal rules the plural lookup is wired to, and 260 bytes
of RAM per catalogue for the formatter table.

## Limits

Define these before including the header. Each catalogue is a fixed struct,
so they decide its RAM cost:

```c
#define CI18N_MAX_KEY_LENGTH 256
#define CI18N_MAX_VALUE_LENGTH 4096
#define CI18N_MAX_LANGUAGES 32
#define CI18N_MAX_KEYS_PER_LANGUAGE 1024
#define CI18N_MAX_CODE_LENGTH 32
#include "ci18n.h"
```

The values shown are the defaults. `CI18N_MAX_LINE_LENGTH`,
`CI18N_MAX_FORMATTERS`, `CI18N_MAX_FORMATTER_NAME` and
`CI18N_MAX_FORMATTER_ARG` can be set the same way. A load that hits a limit
says so in its stats; see
[Getting started](getting-started.md#when-something-goes-wrong).

## Memory

| What | Size |
| --- | --- |
| A catalogue, empty | 3.5 KB, no heap; 0.4 KB with `CI18N_MINIMAL` and 4 languages on a 32-bit target |
| The default catalogue | 3.5 KB of static storage |
| `CI18N_THREAD_LOCAL_CONTEXT` | 3.5 KB per thread that calls `ci18n_init()` |
| Loaded translations | about 1.4 times the file size |

A 1 000-key language from a 66 KB file takes 94 KB of heap, a 10 000-key
one from 664 KB takes 0.9 MB. A load gives back the spare room its buffers
grew into, which costs one copy and makes loading about 20% slower. Nothing
is allocated until you store something.

## Linkage

`CI18N_DEF` decorates every public function. Override it to change how the
library is linked:

```c
#define CI18N_DEF static                  /* keep the API private to one file */
#define CI18N_DEF __declspec(dllexport)   /* export from a Windows DLL */
#define CI18N_DEF __declspec(dllimport)   /* consume that DLL */
```

With `static`, expect `-Wunused-function` for any API you do not call.

## Small devices

The smallest target is an Arduino Uno: 8 bits, 2 KB of RAM, 32 KB of flash.
CI runs the unit tests on an ATmega2560 and on the Uno's ATmega328P under
QEMU, bare metal on a Cortex-M3 with newlib, and on RV32, the ESP32-C3 class,
with picolibc; see [Platforms](platforms.md).

What a device build usually wants:

- **Translations compiled in.** [Compiled catalogues](compiled-catalogs.md)
  cost no RAM at all: three languages on a Cortex-M4 went from 3.2 KB of
  heap to none. Define `CI18N_NO_FILES` if there is no file system.
- **Limits sized to the product.** Four languages and a few dozen keys make
  a catalogue of a few hundred bytes.
- **Text from outside checked first.** `ci18n_utf8_valid` before loading a
  language pack that arrived over the air.

[examples/embedded_ui](../examples/embedded_ui/) is a 20x4 display in three
languages, one of them right to left, built that way.

### Arduino and AVR

The library is an Arduino library and a PlatformIO one. In the Arduino IDE,
use Sketch > Include Library > Add .ZIP Library with a release archive from
GitHub. In PlatformIO, from its [registry](https://registry.platformio.org/libraries/ilyabrin/ci18n):

```ini
lib_deps = ilyabrin/ci18n@^2.18.0
```

Then include `ci18n.h` in the sketch and nothing else. The library compiles
its own implementation, so do not define `CI18N_IMPLEMENTATION` there.
[examples/arduino/Hello](../examples/arduino/Hello/Hello.ino) prints two
languages with plurals to the serial monitor.

Three things are different on AVR, and all three happen by themselves:

- **Small limits.** `CI18N_SMALL_LIMITS` is on: keys of 64 bytes, values of
  256, 4 languages of 64 keys each, which makes a catalogue 0.3 KB. Any
  limit you define yourself still wins, and the macro works on any other
  small target too.
- **Tables in flash.** avr-gcc copies every constant into RAM at startup
  unless told otherwise. The library's own tables, the plural rules and
  number symbols, and every compiled catalogue go to flash.
- **Translations are copied out.** A string in flash cannot be read through a
  plain pointer, so the functions that return one, `ci18n_get()`,
  `ci18n_plural()`, `ci18n_ordinal()` and their relatives, are a compile
  error that names the replacement. Read translations with the functions
  that fill a buffer:

```c
char line[48];

ci18n_get_copy("title", line, sizeof(line));
ci18n_plural_copy("files", count, line, sizeof(line));
ci18n_format_plural(line, sizeof(line), "files", count, NULL);
```

Those work on every platform, so code written this way ports anywhere.
Defining `CI18N_NO_COMPILED` brings the pointer functions back, and leaves
only languages loaded at run time, which live in RAM.

Compile the catalogues on your computer, as for any target; see
[Compiled catalogues](compiled-catalogs.md). A value is copied into a
buffer of `CI18N_MAX_VALUE_LENGTH` bytes, so a catalogue with a longer one
is a build error on AVR rather than a translation cut short. That buffer is
also most of the RAM the library takes: lower the limit to shrink it.

What it costs on an Uno:

| Build | Flash | RAM |
| --- | --- | --- |
| The Hello example: 2 languages, plurals, formatting | 9.5 KB | 0.7 KB |
| `CI18N_MINIMAL`, 2 compiled languages, a lookup and a plural | 6.4 KB | 0.6 KB |

The figures are what the library and its catalogues add to an empty sketch.
Constant data has to sit in the first 64 KB of flash, which avr-gcc arranges
by itself on every board short of a very full Mega.
