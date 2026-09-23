# Coming from gettext

Two ways over: keep your `.po` workflow and load what `msgfmt` builds, or
convert once and leave gettext behind.

- [Load .mo files directly](#load-mo-files-directly)
- [Convert .po files](#convert-po-files)
- [Why there is no .po reader](#why-there-is-no-po-reader)

## Load .mo files directly

Load the `.mo` files you already build, no libintl and no conversion step:

```c
ci18n_load_mo("ru", "locale/ru/LC_MESSAGES/app.mo");
ci18n_set_current("ru");
ci18n_get("Hello, world");     /* keys are the msgids */
ci18n_plural("%d file", n);    /* msgid_plural works too */
```

| gettext | ci18n key |
| --- | --- |
| `msgid "Open"` | `Open` |
| `msgctxt "menu"` with `msgid "Open"` | `menu.Open` |
| `msgid_plural` with `msgstr[0]`, `msgstr[1]`, ... | `msgid[one]`, `msgid[few]`, ... |

Plural forms land on CLDR categories by the header's `nplurals`. Both byte
orders are read, and every offset is checked before use, so a broken file is
refused rather than read past. `ci18n_load_mo_from_buffer` takes one already
in memory. The loader is about 2.5 KB of code; define `CI18N_NO_MO` to leave
it out.

## Convert .po files

To leave gettext entirely, `tools/po2ci18n.py` converts a `.po` catalogue into
a ci18n translation file:

```bash
tools/po2ci18n.py ru.po -o translations/ru.txt
tools/po2ci18n.py --keys=slug ru.po -o translations/ru.txt
```

It handles escapes and the wrapped continuation lines gettext writes, maps
`msgid_plural` and its indexed `msgstr[0]`, `msgstr[1]` onto CLDR category
names, turns `msgctxt` into a key prefix, and skips fuzzy, untranslated and
obsolete entries. `--keys=slug` turns English sentences into short
identifiers, which avoids `CI18N_MAX_KEY_LENGTH`.

Both routes give the same keys. `make test-po` runs the converter over a
sample catalogue and loads the result back, and `make test-mo` compiles the
same sample with `msgfmt`, in both byte orders, and checks that the `.mo`
loader ends up with exactly what the converter wrote.

## Why there is no .po reader

A full one is around 700 lines, 300 of them an evaluator for the C expression
gettext puts in its Plural-Forms header, which would double the parse surface
of a library whose point is one small header. Converting at build time
evaluates that expression on your machine rather than on the device, and the
`.mo` loader needs only the number of forms.
