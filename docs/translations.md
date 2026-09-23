# Translations

What goes in a translation file, and how one key covers "1 file", "3 files"
and "1st place" in every language.

- [The file format](#the-file-format)
- [Escape sequences](#escape-sequences)
- [Plurals](#plurals)
- [Ordinals](#ordinals)
- [Listing every key](#listing-every-key)

## The file format

One `key=value` per line. Blank lines and lines starting with `#` or `;` are
skipped, spaces around the key and the value are trimmed, and a UTF-8 BOM at
the start of the file is ignored. LF, CRLF and a lone CR all end a line.

```ini
# This is a comment
greeting=Hello!
farewell=Goodbye!
error=An error occurred
```

Loading a file that already exists merges into it: new keys are added,
existing ones are replaced, nothing else is touched.

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

## Plurals

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
[tools/cldr_samples.py](../tools/cldr_samples.py).

`ci18n_plural_category(lang, n)` gives the category alone, and
`ci18n_plural_category_name(cat)` its name, for tools that need them. To
fill in the count and other values as well, use `ci18n_format_plural`; see
[Formatting](formatting.md#plurals-with-values).

## Ordinals

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

## Listing every key

`ci18n_foreach(lang, fn, user_data)` calls `fn` once per entry, plural forms
included under their stored keys such as `files[one]`. Return `false` from
the callback to stop early. The order is not defined.

```c
static bool print_entry(const char *key, const char *value, void *user_data)
{
    (void)user_data;
    printf("%s = %s\n", key, value);
    return true;
}

ci18n_foreach("ru", print_entry, NULL);
```

This is what a tool uses to find keys it did not know about;
[examples/cli_sync](../examples/cli_sync/) builds a translation checker on it.
The callback must not call back into the library.
