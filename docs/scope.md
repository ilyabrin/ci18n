# Scope

What ci18n does not do, why, and when to use something else. The boundary is
deliberate. Knowing where it sits should save you an afternoon.

- [Not in scope, and not planned](#not-in-scope-and-not-planned)
- [Why not](#why-not)
- [Which one to pick](#which-one-to-pick)
- [Why this exists](#why-this-exists)

## Not in scope, and not planned

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

The ones marked "through a formatter" are worth a word: the library will not
render a date or a currency, but it will call your function to do it, in the
place the translator chose. See [Formatters](formatting.md#formatters).

## Why not

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

The CLDR data the library does ship, plural and ordinal rules and number
separators, is small, changes rarely, and is checked against CLDR by the unit
tests.

## Which one to pick

- **ci18n**, if you have up to a few hundred strings, one or two people
  translating them, and you need strings rather than date and number
  formatting. Or if size and ease of vendoring matter to you.
- **gettext**, if you have many strings and separate translators, and you want
  `xgettext` so that nobody maintains a list of keys by hand.
- **ICU**, if your interface has dates, currencies, sorting, or
  bidirectional text in earnest.

Nothing here is an argument that ci18n is better. It is smaller, and that is a
different claim.

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
