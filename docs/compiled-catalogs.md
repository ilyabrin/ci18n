# Compiled catalogues

Translations built into the program as constant data: nothing to parse at
startup, no heap, and keys the compiler checks.

- [What it changes](#what-it-changes)
- [Using one](#using-one)
- [In the build](#in-the-build)
- [Checked keys](#checked-keys)
- [Compiled and loaded together](#compiled-and-loaded-together)
- [When not to](#when-not-to)

## What it changes

Measured on the 1 000-key catalogue `make bench` uses, Linux x86-64, gcc
`-O2`, and on [examples/embedded_ui](../examples/embedded_ui/) for a
Cortex-M4:

| | Loaded at run time | Compiled |
| --- | --- | --- |
| Heap for 1 000 keys (a 68 KB file) | 98 KB | none |
| Startup, 1 000 keys | 0.19 ms | under 0.1 µs |
| Startup, 10 000 keys | 2.2 ms | under 0.1 µs |
| `ci18n_get`, key found | 29 ns | 29 ns |
| `ci18n_get`, key missing | 19 ns | 15 ns |
| RAM for three languages on a Cortex-M4 | 3.2 KB of heap | none |
| Flash for them | the text | the text and 0.6 KB of hash tables |

Lookups cost the same: hashing the key is most of the work either way. What
goes is the parsing, the heap, and one more thing: a string from a compiled
language lives as long as the program, so the
[pointer lifetime](getting-started.md#pointer-lifetime) rules do not apply
to it, with or without threads.

The library code for this is 0.8 KB at `-Os` on x86-64 and 0.4 KB on a
Cortex-M4. `CI18N_NO_COMPILED` leaves it out.

## Using one

Build the generator once, then compile a translation file into a header:

```bash
cc -Iinclude -o ci18n_compile tools/ci18n_compile.c
./ci18n_compile -o ru.h ru translations/ru.txt
```

```c
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
#include "ru.h"                                   /* ci18n_compiled_ru */

ci18n_init();
ci18n_use_compiled("ru", &ci18n_compiled_ru);    /* records a pointer */
ci18n_set_current("ru");
puts(ci18n_get("greeting"));                      /* as with any language */
```

Everything that reads works the same: `ci18n_get`, plurals, ordinals,
`ci18n_format`, `ci18n_foreach`, the fallback language, catalogues and all
three threading modes. The file format is the one in
[Translations](translations.md).

The generator reads the file with the library's own loader, so a file means
exactly the same compiled as loaded; the tests check that on every push, on
every platform. What the loader would quietly drop is a build error instead:

```
translations/ru.txt:22: no '=' on the line (1 such lines)
```

A key or value longer than the `CI18N_MAX_*` limits fails the same way. If
your program raises those limits, build the generator with the same
settings.

The generated data does not depend on byte order: generate it on your PC and
build it for any target. A header from a different format version is refused
by `ci18n_use_compiled` with `CI18N_ERR_PARSE`, so regenerate after
upgrading ci18n.

## In the build

**make** or a script: build `ci18n_compile` for the machine you build on, and
run it for each language before compiling the program.

**CMake**: one line per language, and the header is regenerated whenever the
text file changes.

```cmake
ci18n_compile_translations(app ru translations/ru.txt)
ci18n_compile_translations(app en translations/en.txt)
```

This works with ci18n added as a subproject, through FetchContent, or
installed and found with `find_package(ci18n)`. The generator is built from
source the first time it is needed, with your own compiler. When
cross-compiling, for a microcontroller say, it cannot run if it is built for
the target, so build `tools/ci18n_compile.c` for your PC and point
`CI18N_COMPILE_EXECUTABLE` at it.

## Checked keys

The generated header also defines a macro per key, and `CI18N_KEY()` turns a
mistyped key into a compile error:

```c
ci18n_get(CI18N_KEY(greeting));    /* the string "greeting" */
ci18n_get(CI18N_KEY(greting));     /* error: 'ci18n_key_greting' undeclared */
```

A plural key counts once, by its base: `files[one]` and `files[other]` give
`CI18N_KEY(files)`. `CI18N_KEY` is an ordinary string, so it works with
languages loaded at run time too. To check keys without compiling the
translations, generate the key macros alone:

```bash
./ci18n_compile --keys-only -o keys.h en translations/en.txt
```

A key that is not a C identifier, such as `menu.open` or a gettext msgid,
gets no macro; the generator lists them, and `--strict` makes them an error.
Such keys still work as plain strings. `CI18N_KEY` is not a constant
expression, so it cannot initialise a static variable.

## Compiled and loaded together

A catalogue can hold both kinds. A common shape on a device: the languages
that ship with the firmware compiled, a language pack received later loaded
from a buffer, with a compiled language as its fallback.

A compiled language is read only. `ci18n_set`, `ci18n_remove`, `ci18n_clear`
and the loaders refuse it with `CI18N_ERR_READ_ONLY`, rather than copying it
to the heap behind your back, since that copy is the memory compiling it was
meant to save. `ci18n_remove_language` works, and `ci18n_use_compiled` on a
language that was loaded replaces it.

## When not to

- **Translators edit files on the device, or you reload them while running.**
  Compiled languages change only with a new build.
- **You would rather not have a build step.** Loading a text file needs
  nothing but the header, and remains the simplest way to start.

8-bit AVR boards are still not supported. Their flash is read through special
instructions, so a compiled catalogue there would need its own lookup path;
see [Embedded and size](embedded-and-size.md#small-devices).
