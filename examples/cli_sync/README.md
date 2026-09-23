# i18n_check: keep translations in sync

A command-line checker that compares translation files with a reference
language. Put it in CI and a missing key fails the build instead of reaching
users.

```sh
make examples        # builds it as ./example_cli_sync, among others
./example_cli_sync examples/cli_sync/locales/en.txt \
    examples/cli_sync/locales/ru.txt examples/cli_sync/locales/de.txt
```

```
en (reference):
  5 keys
ru:
  5 of 5 keys, ok
de:
  examples/cli_sync/locales/de.txt:4: no '=' on the line, 1 such line(s) skipped
  missing: delete_confirm, users see "Delete {count} photos from {album}?"
  placeholders: greeting has {nmae}, en has {name}
  missing: menu_quit, users see "Quit"
  missing form: photos[one]
  stale: old_banner is gone from en
  3 of 5 keys, needs work
```

The exit status is 0 when every file is clean, 1 when any has a problem and 2
when a file cannot be read. `de.txt` is broken on purpose, so the run above
exits 1.

## What it shows

| Feature | Where |
| --- | --- |
| A catalogue of its own, `ci18n_create` | `main` |
| Loading from a file, and what the load dropped: `ci18n_last_load_stats_in` | `load` |
| Walking every key of a language: `ci18n_foreach_in` | `check` |
| Which plural forms a language needs: `ci18n_plural_category` | `needed_categories` |
| What users see for a missing key, through the fallback | `check` |

## Worth knowing

- **The language decides the plural forms, not the reference.** English has
  `one` and `other`, Russian needs `one`, `few` and `many`. The checker asks
  the library for the category of 0 to 200 and collects what comes back,
  which finds every form a language uses for whole numbers.
- **Placeholders are compared by name.** `{when:date}` and `{when}` count as
  the same placeholder, since the formatter is the translator's choice.
- **Coming from gettext?** Convert first with `tools/po2ci18n.py`, then check
  the result.
