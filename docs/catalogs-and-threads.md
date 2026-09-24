# Catalogues and threads

Using ci18n inside a library without fighting the application over the
current language, and using it from more than one thread.

- [Catalogues](#catalogues)
- [Threads](#threads)
- [The shared mode](#the-shared-mode)

## Catalogues

Everything in [Getting started](getting-started.md) works on one catalogue
the library owns. Convenient for a program, wrong for a library: if ci18n is
used inside a reusable component, the component and the application that
linked it share one current language, and whichever called
`ci18n_set_current()` last wins.

So every function has an `_in` variant taking a catalogue explicitly, and the
plain names are those variants applied to a default one:

```c
ci18n_t *ui   = ci18n_create();
ci18n_t *logs = ci18n_create();

ci18n_load_language_in(ui, "ru", "ru.txt");
ci18n_set_current_in(ui, "ru");

ci18n_load_language_in(logs, "en", "en.txt");
ci18n_set_current_in(logs, "en");

puts(ci18n_get_or_key_in(ui, "greeting"));     /* Russian */
puts(ci18n_get_or_key_in(logs, "greeting"));   /* English */

ci18n_destroy(ui);
ci18n_destroy(logs);
```

A catalogue carries its own languages, its own current and fallback selection,
its own [formatters](formatting.md#formatters), and in the shared threading
mode its own lock, so two of them never wait on each other. `ci18n_default()`
returns the one the plain functions use, so code written against the `_in`
functions can still reach it.

`ci18n_destroy(NULL)` is a no-op, so a failed create needs no special case.

## Threads

Three modes. Pick one before including the header; defining two is an error.

| Mode | What you get |
| --- | --- |
| nothing defined | One global context, no locking |
| `CI18N_THREAD_LOCAL_CONTEXT` | A separate context per thread |
| `CI18N_THREAD_SHARED` | One shared context behind a reader-writer lock |

**Default.** Safe when every load finished before the threads started and they
only read afterwards, which covers most programs. A concurrent `ci18n_set()`
or `ci18n_load_*()` against a concurrent `ci18n_get()` is a data race.

**`CI18N_THREAD_LOCAL_CONTEXT`.** Isolation, not sharing. Each thread calls
`ci18n_init()` and loads its own translations, and a language loaded on one
thread is invisible to the others. Suits a worker rendering in one user's
locale. `CI18N_THREAD_SAFE` is the old name for this one; it still works and
emits a deprecation note.

**`CI18N_THREAD_SHARED`.** One context, an rwlock around every entry point.
Readers do not block each other, a writer excludes everyone. This is the
"load once, read from many threads, reload occasionally" case.

## The shared mode

```c
#define CI18N_THREAD_SHARED
#define CI18N_IMPLEMENTATION
#include "ci18n.h"
```

Three things to know about it.

**Use `ci18n_get_copy()`, not `ci18n_get()`.** A lock cannot make a returned
pointer safe: the moment it is released, a writer may reallocate the storage
that pointer refers to. `ci18n_get_copy()` copies while the read lock is still
held, and so do `ci18n_format` and its relatives.

```c
char text[128];
ci18n_get_copy("greeting", text, sizeof(text));
```

**Diagnostics are per-thread.** `ci18n_last_error()` and
`ci18n_last_load_stats()` describe the calling thread's last call, not the
context's. Sharing one slot would mean two threads overwriting each other.

**Reload by loading on top.** A load merges into the language under one
write lock, so a reader sees the old text or the new one. Clearing first and
loading after is two locks, and a reader can land in between and find the
key gone.

On glibc this mode needs `-D_POSIX_C_SOURCE=200809L`, or `-std=gnu99` instead
of `-std=c99`, because strict ANSI mode hides the POSIX threading
declarations. The header says so with an `#error` if you forget.

**Reads scale with cores.** A catalogue holds 16 locks, each on its own
cache line, and every thread reads through its own one, so readers never
touch the same memory. With `make bench-threads`, one thread does 26 million
`ci18n_get` calls a second and eight threads 146 million between them, no
fewer than eight separate catalogues manage, at 129 million. The price is on the writing side: a
writer takes all 16 locks, so `ci18n_set()` and the loaders cost a little
more under this mode, which suits "load once, reload rarely".

A server usually wants one catalogue per language, since a catalogue has one
current language and a server serves them all at once.
[examples/server](../examples/server/) shows that shape, with a reload under
four readers.
