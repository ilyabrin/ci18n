# A settings screen on a small device

A 20 by 4 character display in English, Russian and Arabic, compiled into
the firmware, then in French from a language pack received at run time.

```sh
make examples        # builds it as ./example_ui with -Os, among others
./example_ui
```

The three built-in languages are [locales/](locales/)`*.txt`, turned into
headers by [tools/ci18n_compile](../../tools/ci18n_compile.c) as part of the
build. By hand:

```sh
cc -Iinclude -o ci18n_compile tools/ci18n_compile.c
./ci18n_compile -o en.h en examples/embedded_ui/locales/en.txt   # and ru, ar
```

```
ru, ltr
+--------------------+
|< Настройки         |
|Яркость          80%|
|Осталась 21 мин     |
|Готово обновление п…|
+--------------------+
ar, rtl
+--------------------+
|         الإعدادات >|
|80%           السطوع|
|     21 دقيقة متبقية|
|تحديث البرنامج الثا…|
+--------------------+
```

## What it shows

| Feature | Where |
| --- | --- |
| Limits sized to the product, set before the include | top of `ui.c` |
| Translations compiled in: `ci18n_use_compiled`, no heap | `main`, [locales/](locales/) |
| A compiled language as the fallback for a loaded one | the French screen |
| A pack from outside checked with `ci18n_utf8_valid` before loading | `install_pack` |
| Keys the pack lacks, filled from the fallback | the French screen |
| Fitting text to columns: `ci18n_utf8_length`, `ci18n_utf8_sequence_length` | `fit` |
| A string in a 16-byte slot, cut with `ci18n_utf8_truncate` | `store_owner` |
| Mirrored layout for right-to-left: `ci18n_current_direction` | `row`, `draw` |
| Arabic writing four of its six plural forms, the rest from `[other]` | [locales/ar.txt](locales/ar.txt) |

## Size

With the limits in `ui.c` the whole context is about 600 bytes of RAM; the
program prints the figure on stderr. Compiling the translations in, measured
on a Cortex-M4 with `-Os` and `--gc-sections`:

| | Loaded from buffers | Compiled |
| --- | --- | --- |
| Heap for the three languages | 3 232 bytes | none |
| Flash, the whole program | 21 053 bytes | 21 673 bytes |

The translations are 803 bytes of text, and cost four times that in RAM once
loaded, for entry arrays, hash buckets and spare room. Compiled, they cost
nothing in RAM and 620 bytes more flash for their hash tables.

## Worth knowing

- **Truncate, then copy.** Copying into the slot first cuts at the last byte,
  maybe inside a letter, and what is left already fits, so there is nothing
  for `ci18n_utf8_truncate` to repair. `store_owner` does it in the right
  order.
- **A code point is not always one cell.** Counting code points is right for
  a character display. A graphical one with wide CJK glyphs or combining
  marks needs a real width function.
- **Mirroring the layout is the library's part, and ordering the glyphs is
  not.** Inside an Arabic run, characters are stored in reading order. A
  terminal reorders them for you; a bare display controller needs a bidi
  step, or text prepared for it.
