# Formatting

Putting values into translations: names, counts, dates you render yourself,
and numbers written the way each language writes them.

- [Placeholders](#placeholders)
- [Plurals with values](#plurals-with-values)
- [Buffers and truncation](#buffers-and-truncation)
- [Formatters](#formatters)
- [Numbers](#numbers)

Everything on this page is left out by `CI18N_NO_FORMAT`, and numbers by
`CI18N_NO_NUMBERS`; see [Embedded and size](embedded-and-size.md).

## Placeholders

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

The arguments after the key are name and value pairs, ended by `NULL`.

Two deliberate choices worth knowing:

- **Values are strings, never a format string.** A translation file is data,
  often written by someone who is not the programmer. Passing it to
  `printf()` as a format would make every translator a potential attacker.
- **An unmatched placeholder stays as written.** `{nmae}` comes out as
  `{nmae}`, so a typo is visible rather than a silent hole.

Write `{{` and `}}` for literal braces.

## Plurals with values

Together with [plurals](translations.md#plurals), which is where it earns its
keep:

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
the right form. `ci18n_format_ordinal` does the same for
[ordinals](translations.md#ordinals).

## Buffers and truncation

The formatting functions follow `snprintf()`: at most `capacity - 1` bytes are
written, the result is always terminated, and the return value is the length
the whole result would have had, so a return of `capacity` or more means
truncation. `ci18n_format(NULL, 0, ...)` measures without writing.

A truncated result is always valid UTF-8 and always a prefix of the full one:
the cut never lands inside a character.

```c
char out[8];

ci18n_set("ru", "greeting", "Привет");
ci18n_format(out, sizeof(out), "greeting", NULL);  /* returns 12 */
/* out is "При", 6 bytes, not 7 bytes ending in half a character */
```

## Formatters

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

## Numbers

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
  in under 1 KB. [tools/cldr_numbers.py](../tools/cldr_numbers.py) generates
  the table and checks every language against CLDR.
- **The current language decides.** An unknown one is written the English
  way, as it gets English plurals.
- **Anything else passes through.** `1e9` or `12,5` comes out as it went in,
  and a formatter you register as `number` replaces the built-in one.

Digits stay 0 to 9 everywhere: Arabic, Persian, Bengali and a few others
have native digits of their own, and CLDR lists Latin ones for every
language as well, which is what this writes.
