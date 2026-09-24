# API reference

Every public function, grouped by what it is for. The header documents each
one in full next to its declaration; this page is the map.

Every function taking or returning text works in UTF-8. Functions that write
into a buffer follow `snprintf()`: they write at most `capacity - 1` bytes,
always terminate, and return the length the whole result would have had.

- [Setup](#setup)
- [Loading](#loading)
- [Choosing a language](#choosing-a-language)
- [Lookup](#lookup)
- [Editing](#editing)
- [Plurals and ordinals](#plurals-and-ordinals)
- [Formatting](#formatting)
- [Text direction](#text-direction)
- [UTF-8](#utf-8)
- [Diagnostics](#diagnostics)
- [Catalogues](#catalogues)
- [Errors](#errors)

The last column names the macro that leaves a function out, if any; see
[Leaving parts out](embedded-and-size.md#leaving-parts-out). AVR there means
the function returns a translation as a pointer, which on AVR can point into
flash, so calling it is a compile error; the `_copy` functions replace it.
See [Arduino and AVR](embedded-and-size.md#arduino-and-avr).

## Setup

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_init()` | Initialise the default catalogue; every other call fails before this | |
| `ci18n_free()` | Free everything it holds | |
| `ci18n_is_initialized()` | Whether `ci18n_init()` has run | |

## Loading

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_load_language(code, path)` | Load a `key=value` file, merging into the language | `NO_FILES` |
| `ci18n_load_from_buffer(code, text, len)` | The same, from memory | |
| `ci18n_load_mo(code, path)` | Load a gettext `.mo` file | `NO_MO`, `NO_FILES` |
| `ci18n_load_mo_from_buffer(code, data, len)` | The same, from memory | `NO_MO` |
| `ci18n_use_compiled(code, &compiled)` | Use a language built by `tools/ci18n_compile`; copies nothing | `NO_COMPILED` |
| `CI18N_KEY(name)` | The key as a string, a compile error if the generated keys lack it | |

## Choosing a language

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_set_current(code)` | Pick the language lookups use | |
| `ci18n_set_fallback(code)` | Pick the language that answers what the current one lacks | |
| `ci18n_get_current()` | The current language's code | |
| `ci18n_get_languages(out, cap)` | The loaded languages; returns how many | |
| `ci18n_detect_locale(out, cap)` | The system's locale, normalised: `ru-RU` | `NO_LOCALE` |
| `ci18n_set_current_best(locale)` | Current language by best match, `ru-RU` then `ru`; `NULL` asks the system | `NO_LOCALE` |

## Lookup

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_get(key)` | The translation, or `NULL` | AVR |
| `ci18n_get_or_key(key)` | The translation, or the key itself | AVR |
| `ci18n_get_copy(key, out, cap)` | Copy it into your buffer; the safe one with threads, and the one for AVR | |
| `ci18n_has(key)` | Whether the current or fallback language has it | |
| `ci18n_count(code)` | Entries in a language | |
| `ci18n_foreach(code, fn, user_data)` | Call `fn` for every entry | |

## Editing

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_set(code, key, value)` | Add or replace one entry | |
| `ci18n_remove(code, key)` | Remove one entry | |
| `ci18n_clear(code)` | Empty a language, keeping it loaded | |
| `ci18n_remove_language(code)` | Unload a language and free its slot | |

## Plurals and ordinals

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_plural(key, n)` | The plural form for `n`, or `NULL` | AVR |
| `ci18n_plural_or_key(key, n)` | The same, or the key | AVR |
| `ci18n_plural_copy(key, n, out, cap)` | Copy the plural form into your buffer | |
| `ci18n_plural_category(code, n)` | The CLDR category of `n` | |
| `ci18n_plural_category_name(cat)` | `"one"`, `"few"` and so on | |
| `ci18n_ordinal(key, n)` | The ordinal form: 1st, 2nd, 3rd | `NO_ORDINALS`, AVR |
| `ci18n_ordinal_or_key(key, n)` | The same, or the key | `NO_ORDINALS`, AVR |
| `ci18n_ordinal_copy(key, n, out, cap)` | Copy the ordinal form into your buffer | `NO_ORDINALS` |
| `ci18n_ordinal_category(code, n)` | The CLDR ordinal category of `n` | `NO_ORDINALS` |

## Formatting

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_format(out, cap, key, name, value, ..., NULL)` | Fill `{name}` placeholders | `NO_FORMAT` |
| `ci18n_format_plural(out, cap, key, n, ..., NULL)` | Plural form, filled; `{count}` is `n` | `NO_FORMAT` |
| `ci18n_format_ordinal(out, cap, key, n, ..., NULL)` | Ordinal form, filled | `NO_FORMAT`, `NO_ORDINALS` |
| `ci18n_set_formatter(name, fn, user_data)` | Register a renderer for `{value:name}` | `NO_FORMAT` |
| `ci18n_remove_formatter(name)` | Forget one | `NO_FORMAT` |
| `ci18n_format_number(out, cap, code, number, digits)` | A number the language's way | `NO_NUMBERS` |
| `ci18n_set_bidi_isolation(on)` | Wrap filled-in values in FSI and PDI | `NO_FORMAT` |

## Text direction

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_direction(code)` | `CI18N_DIR_LTR` or `CI18N_DIR_RTL` for any code | |
| `ci18n_current_direction()` | The same for the current language | |
| `ci18n_direction_name(dir)` | `"ltr"` or `"rtl"`, for HTML and CSS | |
| `ci18n_bidi_isolate(out, cap, text)` | Wrap one string in FSI and PDI | |
| `ci18n_bidi_mark(dir)` | LRM or RLM | |

## UTF-8

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_utf8_valid(text)` | Strictly well-formed UTF-8 | |
| `ci18n_utf8_length(text)` | Characters, not bytes | |
| `ci18n_utf8_sequence_length(text)` | Bytes in the character here; never 0 | |
| `ci18n_utf8_truncate(text, max)` | Cut to at most `max` bytes, between characters | |

## Diagnostics

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_last_error()` | Why the calling thread's last call failed | |
| `ci18n_error_string(err)` | That, as text | |
| `ci18n_last_load_stats()` | What the last load read, dropped and cut | |

## Catalogues

| Function | Does | Left out by |
| --- | --- | --- |
| `ci18n_create()` | A new, empty catalogue | |
| `ci18n_destroy(catalog)` | Free one; `NULL` is fine | |
| `ci18n_default()` | The catalogue the plain functions use | |
| `ci18n_*_in(catalog, ...)` | Every function above that works on translations, on that catalogue | as above |
| `ci18n_get_context()` | The calling thread's catalogue, with `CI18N_THREAD_LOCAL_CONTEXT` only | |

## Errors

`ci18n_last_error()` returns one of these, and `ci18n_error_string()` turns it
into text.

| Code | Meaning |
| --- | --- |
| `CI18N_OK` | The last call succeeded |
| `CI18N_ERR_NOT_INITIALIZED` | `ci18n_init()` has not been called |
| `CI18N_ERR_INVALID_ARGUMENT` | A `NULL` or otherwise unusable argument |
| `CI18N_ERR_CODE_TOO_LONG` | A language code longer than `CI18N_MAX_CODE_LENGTH` |
| `CI18N_ERR_FILE_NOT_FOUND` | The file could not be opened |
| `CI18N_ERR_OUT_OF_MEMORY` | An allocation failed |
| `CI18N_ERR_TOO_MANY_LANGUAGES` | `CI18N_MAX_LANGUAGES` reached |
| `CI18N_ERR_TOO_MANY_KEYS` | `CI18N_MAX_KEYS_PER_LANGUAGE` reached |
| `CI18N_ERR_LANGUAGE_NOT_FOUND` | No such language is loaded |
| `CI18N_ERR_KEY_NOT_FOUND` | No such key in the languages consulted |
| `CI18N_ERR_PARSE` | A load dropped or cut something, or a file is not the format asked for |
| `CI18N_ERR_TOO_MANY_FORMATTERS` | `CI18N_MAX_FORMATTERS` reached |
| `CI18N_ERR_UNKNOWN_FORMATTER` | A translation asked for a formatter nobody registered |
| `CI18N_ERR_READ_ONLY` | The language is compiled in and cannot be changed |
