# A settings screen on a small device

A 20 by 4 character display in English, Russian and Arabic, then in French
from a language pack received at run time.

```sh
make examples        # builds it as ./example_ui with -Os, among others
./example_ui
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
| Translations compiled in, loaded from buffers | `main` |
| A pack from outside checked with `ci18n_utf8_valid` before loading | `install_pack` |
| Keys the pack lacks, filled from the fallback | the French screen |
| Fitting text to columns: `ci18n_utf8_length`, `ci18n_utf8_sequence_length` | `fit` |
| A string in a 16-byte slot, cut with `ci18n_utf8_truncate` | `store_owner` |
| Mirrored layout for right-to-left: `ci18n_current_direction` | `row`, `draw` |
| Arabic writing four of its six plural forms, the rest from `[other]` | `AR` |

## Size

With the limits in `ui.c` the whole context is under 600 bytes of RAM before
any translation is loaded; the program prints the figure on stderr. The
library adds 18 to 19 KB of code at `-Os`.

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
