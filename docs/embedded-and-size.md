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
| Everything | 38 KB | 27 KB | 13 KB |
| `CI18N_MINIMAL` | 21 KB | 14 KB | 7 KB |

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
| `CI18N_MINIMAL` | all of the above | 13 / 6.0 KB, about half |

```c
#define CI18N_MINIMAL
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

What stays in every build: loading from buffers, lookup, plurals, the
fallback language, catalogues, text direction, the UTF-8 helpers and the
diagnostics. CI builds and tests each of these on its own, and all together.

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

The smallest target is a 32-bit microcontroller with 64 KB of RAM. CI runs the
unit tests bare metal on a Cortex-M3 with newlib and on RV32, the ESP32-C3
class, with picolibc; see [Platforms](platforms.md).

8-bit AVR boards such as the Arduino Uno are not supported: the library keeps
strings on the heap and needs `fopen` for its file loaders, and an Uno has
2 KB of RAM.

What a device build usually wants:

- **Translations compiled in.** Load them from a `const char[]` with
  `ci18n_load_from_buffer`, and define `CI18N_NO_FILES` if there is no file
  system.
- **Limits sized to the product.** Four languages and a few dozen keys make
  a catalogue of a few hundred bytes.
- **Text from outside checked first.** `ci18n_utf8_valid` before loading a
  language pack that arrived over the air.

[examples/embedded_ui](../examples/embedded_ui/) is a 20x4 display in three
languages, one of them right to left, built that way.
