# A threaded server

A server without the network: four worker threads take requests off a queue,
choose a language from each `Accept-Language` header and render a reply. A
reload thread then rewrites a translation 200 times while four readers keep
reading it.

```sh
make examples        # builds it as ./example_server, among others
./example_server
```

```
[3] ar-EG
  lang=ar dir=rtl
  مرحبا بعودتك
  ليلى، لديك 11 رسالة جديدة
  You are 3rd in line
  الرصيد: 42.00 USD
...
reload: 200 reloads under 4 readers, 0 torn reads
ru motd is now: Добро пожаловать снова
```

Needs pthreads, so it builds on Linux, macOS and the BSDs, and on Windows with
MinGW.

## The shape to copy

1. **`CI18N_THREAD_SHARED`.** Any thread may read, and one may write while
   they do. Reads scale with cores, see the README's Threads section.
2. **One catalogue per language.** A catalogue has one current language and
   a server serves all of them at once, so a request picks a catalogue, not
   a language. Each catalogue loads English as its fallback.
3. **Copy, do not borrow.** `ci18n_get_copy_in` and the `ci18n_format_*_in`
   functions copy while the lock is held. A pointer from `ci18n_get_in` can
   dangle the moment a reload runs.
4. **Reload by loading on top.** A load merges into the language under one
   write lock, so a reader sees the old text or the new one. Clearing first
   and loading after is two locks, and a reader can land in between and find
   the key gone.

## What it shows

| Feature | Where |
| --- | --- |
| Catalogues: `ci18n_create`, `ci18n_*_in` | `setup` |
| `Accept-Language` with weights, then the primary subtag | `negotiate` |
| Plurals, ordinals, placeholders | `handle` |
| A formatter with per-language settings in `user_data` | `format_money` |
| Text direction for `<html dir>` | `handle` |
| A key missing in Arabic, served from English with English grammar | request 3 |
| Reloading under readers | `reloader`, `reader` |
