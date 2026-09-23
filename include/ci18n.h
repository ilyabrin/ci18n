/*
 * ci18n.h - v2.16.0
 * Single-header internationalization (i18n) library for C projects
 *
 * Features:
 *   - Simple translation key-value storage
 *   - Multiple language support
 *   - UTF-8 string handling, BOM tolerant
 *   - Explicit catalogues, so a library can use it without stealing
 *     the application's language
 *   - Three threading modes, including a shared context behind an rwlock
 *   - No external dependencies, C99 and newer
 *   - Packed storage: an entry costs 16 bytes, not 4352
 *   - CLDR plural rules, so Russian and Arabic work, not just English
 *   - CLDR ordinal rules, so 1st, 2nd, 3rd and 11th come out right
 *   - Locale detection with a fallback chain: ru-RU to ru
 *   - Named interpolation, so translations decide where values go
 *   - Text direction, so an Arabic or Hebrew interface lays out correctly
 *   - UTF-8 helpers, and truncation that never splits a character
 *   - Pluggable formatters, so dates and numbers stay your code
 *
 * USAGE:
 *   #define CI18N_IMPLEMENTATION before including this header in ONE source file
 *
 * EXAMPLE:
 *   #define CI18N_IMPLEMENTATION
 *   #include "ci18n.h"
 *
 *   int main() {
 *       ci18n_init();
 *       ci18n_load_language("en", "translations_en.txt");
 *       ci18n_set_current("en");
 *       printf("%s\n", ci18n_get("welcome_message"));
 *       ci18n_free();
 *       return 0;
 *   }
 *
 * TRANSLATION FILE FORMAT (plain text, one per line):
 *   key=value
 *   # This is a comment
 *   welcome_message=Welcome to our application!
 *   multiline=First line.
Second line.
 *
 * Copyright (c) 2026 Ilya Brin
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CI18N_H
#define CI18N_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * Version
 * ============================================================================ */

#define CI18N_VERSION_MAJOR 2
#define CI18N_VERSION_MINOR 16
#define CI18N_VERSION_PATCH 0
#define CI18N_VERSION_STRING "2.16.0"

/* Compare against this to require a minimum version at compile time:
 *   #if CI18N_VERSION < CI18N_VERSION_NUMBER(2, 0, 0)
 *   #error "ci18n 2.0.0 or newer is required"
 *   #endif
 */
#define CI18N_VERSION_NUMBER(major, minor, patch) ((major) * 10000 + (minor) * 100 + (patch))
#define CI18N_VERSION CI18N_VERSION_NUMBER(CI18N_VERSION_MAJOR, CI18N_VERSION_MINOR, CI18N_VERSION_PATCH)

/* ============================================================================
 * Linkage
 * ============================================================================ */

/* Decoration applied to every public function. Override it before including
 * this header to change how the library is linked:
 *
 *   #define CI18N_DEF static          keep the API private to one file
 *   #define CI18N_DEF __declspec(dllexport)   export from a Windows DLL
 *   #define CI18N_DEF __declspec(dllimport)   consume that DLL
 *
 * With `static`, expect -Wunused-function for any API you do not call. That is
 * normal for a private build; silence it per translation unit if it bothers you.
 */
#ifndef CI18N_DEF
#define CI18N_DEF extern
#endif

/* ============================================================================
 * Deprecated macros
 * ============================================================================ */

/* CI18N_THREAD_SAFE was a misleading name: it never added locking, it gave
 * each thread a separate context. Kept working, but prefer the new name. */
#ifdef CI18N_THREAD_SAFE
#ifndef CI18N_THREAD_LOCAL_CONTEXT
#define CI18N_THREAD_LOCAL_CONTEXT
#endif
#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
#pragma message("ci18n: CI18N_THREAD_SAFE is deprecated, use CI18N_THREAD_LOCAL_CONTEXT")
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /* ============================================================================
     * Configuration
     * ============================================================================ */

#ifndef CI18N_MAX_KEY_LENGTH
#define CI18N_MAX_KEY_LENGTH 256
#endif

#ifndef CI18N_MAX_VALUE_LENGTH
#define CI18N_MAX_VALUE_LENGTH 4096
#endif

#ifndef CI18N_MAX_LANGUAGES
#define CI18N_MAX_LANGUAGES 32
#endif

#ifndef CI18N_MAX_KEYS_PER_LANGUAGE
#define CI18N_MAX_KEYS_PER_LANGUAGE 1024
#endif

/* Wide enough to hold a maximum key and a maximum value on one line, plus the
 * separator and some slack. When this is not larger than
 * CI18N_MAX_VALUE_LENGTH, the line limit bites first and a long value can
 * never reach its own limit. */
#ifndef CI18N_MAX_LINE_LENGTH
#define CI18N_MAX_LINE_LENGTH (CI18N_MAX_KEY_LENGTH + CI18N_MAX_VALUE_LENGTH + 8)
#endif

/* Includes the terminator, so the default fits a 31 character code. Long
 * enough for anything BCP 47 produces in practice, such as ca-ES-valencia. */
#ifndef CI18N_MAX_CODE_LENGTH
#define CI18N_MAX_CODE_LENGTH 32
#endif

/*
 * Leave parts out. Each of these removes a feature, its code and its data,
 * and the matching functions stop being declared, so a call to one is a
 * compile error rather than a surprise. Nothing is left out by default.
 *
 *   CI18N_NO_FORMAT    ci18n_format and relatives, formatters, bidi isolation
 *   CI18N_NO_NUMBERS   ci18n_format_number and the {n:number} formatter
 *   CI18N_NO_ORDINALS  ci18n_ordinal and relatives
 *   CI18N_NO_LOCALE    ci18n_detect_locale and ci18n_set_current_best
 *   CI18N_NO_MO        the gettext .mo loader
 *   CI18N_NO_FILES     every loader that opens a file; buffers still load
 *   CI18N_MINIMAL      all of the above
 *
 * What stays in any build: loading from buffers, lookup, plurals, fallback,
 * catalogues, text direction, the UTF-8 helpers and the diagnostics.
 */
#ifdef CI18N_MINIMAL
#ifndef CI18N_NO_FORMAT
#define CI18N_NO_FORMAT
#endif
#ifndef CI18N_NO_NUMBERS
#define CI18N_NO_NUMBERS
#endif
#ifndef CI18N_NO_ORDINALS
#define CI18N_NO_ORDINALS
#endif
#ifndef CI18N_NO_LOCALE
#define CI18N_NO_LOCALE
#endif
#ifndef CI18N_NO_MO
#define CI18N_NO_MO
#endif
#ifndef CI18N_NO_FILES
#define CI18N_NO_FILES
#endif
#endif

/* How many formatters one catalogue can hold. Applications register a handful
 * at startup, so the table is small and lives in the catalogue rather than on
 * the heap. */
#ifndef CI18N_MAX_FORMATTERS
#define CI18N_MAX_FORMATTERS 8
#endif

/* Includes the terminator. Formatter names are written by you, not by
 * translators, so they are short: "date", "number", "currency". */
#ifndef CI18N_MAX_FORMATTER_NAME
#define CI18N_MAX_FORMATTER_NAME 24
#endif

/* Includes the terminator. The argument after the comma in
 * {when:date,long} is a form name or a pattern, not prose, so this is
 * generous. A longer one makes the translation malformed rather than
 * silently shortened. */
#ifndef CI18N_MAX_FORMATTER_ARG
#define CI18N_MAX_FORMATTER_ARG 64
#endif

/* ============================================================================
 * Threading primitives
 * ============================================================================
 *
 * Declared up here rather than with the implementation because the lock is a
 * field of the catalogue, so its type has to be complete where the struct is.
 * A consumer asking for the shared mode therefore pulls in pthread.h, or
 * windows.h on Windows, which is the price of the lock living where it
 * belongs.
 * ============================================================================ */

#if defined(CI18N_THREAD_LOCAL_CONTEXT) && defined(CI18N_THREAD_SHARED)
#error "CI18N_THREAD_LOCAL_CONTEXT and CI18N_THREAD_SHARED are alternatives, not a pair"
#endif

#if defined(CI18N_THREAD_LOCAL_CONTEXT) || defined(CI18N_THREAD_SHARED)
/* Pick the storage keyword by compiler, not by OS.
 *
 * MinGW gcc defines _WIN32 but rejects __declspec(thread): it ignores the
 * attribute with a warning, which silently turns the per-thread context back
 * into one shared global. Ask the compiler what it speaks instead. */
#if defined(_MSC_VER)
#define CI18N_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define CI18N_THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define CI18N_THREAD_LOCAL _Thread_local
#else
#error "this mode requires a compiler with thread-local storage"
#endif
#endif

/* ---------------------------------------------------------------------------
 * The reader-writer lock for CI18N_THREAD_SHARED.
 *
 * It lives inside the context rather than beside the global, so that when the
 * API grows an explicit handle the lock travels with it and each handle locks
 * itself. Statically initialised, which also removes the question of who
 * initialises it first: there is no window before ci18n_init().
 * --------------------------------------------------------------------------- */
#ifdef CI18N_THREAD_SHARED

/* For the abort path below, which has to work wherever the lock is used. */
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
typedef SRWLOCK ci18n_rwlock_t;
#define CI18N_RWLOCK_INIT SRWLOCK_INIT
/* These return void: there is nothing to check. */
#define ci18n_rwlock_read(l) AcquireSRWLockShared(l)
#define ci18n_rwlock_read_unlock(l) ReleaseSRWLockShared(l)
#define ci18n_rwlock_write(l) AcquireSRWLockExclusive(l)
#define ci18n_rwlock_write_unlock(l) ReleaseSRWLockExclusive(l)
#else

/* glibc gates pthread_rwlock_* behind __USE_XOPEN2K, which needs
 * _POSIX_C_SOURCE at 200112L or above. Strict ANSI mode, which -std=c99
 * selects, does not reach it, and neither does -pthread on its own: that sets
 * _POSIX_C_SOURCE to 199506L, so testing whether the macro is merely defined
 * is not enough. Checking glibc's own gate is, and it turns an implicit
 * declaration into an answer. */
#if defined(__GLIBC__) && !defined(__USE_XOPEN2K)
#error "CI18N_THREAD_SHARED on glibc needs -D_POSIX_C_SOURCE=200809L, or -std=gnu99 instead of -std=c99"
#endif

#include <pthread.h>
typedef pthread_rwlock_t ci18n_rwlock_t;
#define CI18N_RWLOCK_INIT PTHREAD_RWLOCK_INITIALIZER

/*
 * The return codes are checked, and a failure aborts.
 *
 * Not defensive programming: an rwlock call only fails when the lock itself is
 * unusable, and carrying on then means running with no mutual exclusion at
 * all. That is precisely how the macOS bug in 2.6.0 stayed hidden, where a
 * lock that was zeroed rather than properly initialised returned EINVAL from
 * every call and silently did nothing. Dying loudly is better than
 * pretending.
 *
 * Define CI18N_LOCK_FAILED yourself to handle it some other way.
 */
#ifndef CI18N_LOCK_FAILED
#define CI18N_LOCK_FAILED(what)                                          \
    do                                                                   \
    {                                                                    \
        fprintf(stderr, "ci18n: %s failed, the catalogue is unprotected " \
                        "and continuing would be worse\n", what);        \
        abort();                                                         \
    } while (0)
#endif

#define ci18n_rwlock_read(l)                     \
    do                                           \
    {                                            \
        if (pthread_rwlock_rdlock(l) != 0)       \
        {                                        \
            CI18N_LOCK_FAILED("read lock");      \
        }                                        \
    } while (0)

#define ci18n_rwlock_write(l)                    \
    do                                           \
    {                                            \
        if (pthread_rwlock_wrlock(l) != 0)       \
        {                                        \
            CI18N_LOCK_FAILED("write lock");     \
        }                                        \
    } while (0)

#define ci18n_rwlock_read_unlock(l)              \
    do                                           \
    {                                            \
        if (pthread_rwlock_unlock(l) != 0)       \
        {                                        \
            CI18N_LOCK_FAILED("unlock");         \
        }                                        \
    } while (0)

#define ci18n_rwlock_write_unlock(l) ci18n_rwlock_read_unlock(l)
#endif

/*
 * One lock per catalogue does not scale: every reader writes to the same
 * lock word, so readers on different cores fight over one cache line even
 * though they never block each other. The catalogue holds 16 locks instead,
 * each on its own cache line. A reader takes the one its thread was given,
 * and a writer takes all of them, in order. Reads get faster with every
 * core; writes cost 16 lock operations, which a reload can afford.
 */
#define CI18N_LOCK_SHARDS 16

typedef union
{
    ci18n_rwlock_t lock;
    char line[(sizeof(ci18n_rwlock_t) + 63) / 64 * 64];
} ci18n_lock_shard_t;

#define CI18N_SHARD_INIT_ {CI18N_RWLOCK_INIT}
#define CI18N_SHARD_INIT_4_ CI18N_SHARD_INIT_, CI18N_SHARD_INIT_, CI18N_SHARD_INIT_, CI18N_SHARD_INIT_
#define CI18N_LOCK_SHARDS_INIT     {CI18N_SHARD_INIT_4_, CI18N_SHARD_INIT_4_, CI18N_SHARD_INIT_4_, CI18N_SHARD_INIT_4_}

#endif /* CI18N_THREAD_SHARED */

    /* ============================================================================
     * Types
     * ============================================================================ */

    /* ------------------------------------------------------------------------
     * Storage internals
     * ------------------------------------------------------------------------
     *
     * These types are visible because ci18n_get_context() hands the context
     * out, not because their layout is part of the API. Reach for the
     * functions instead: the shapes here changed once already and may again.
     *
     * Keys and values live packed in a per-language arena and are referenced
     * by offset rather than by pointer, so growing the arena does not have to
     * fix anything up. An entry is 16 bytes, against the 4352 a pair of fixed
     * fields used to cost, and an empty language holds no heap at all until
     * the first insert.
     * ------------------------------------------------------------------------ */

    /* Marks "no entry" in a bucket or a chain link. */
#define CI18N_NO_INDEX UINT32_MAX

    /* Packed, NUL-terminated strings addressed by offset. */
    typedef struct ci18n_arena
    {
        char *data;
        size_t used;
        size_t capacity;
    } ci18n_arena_t;

    typedef struct ci18n_entry
    {
        uint32_t key;   /* offset into the language arena */
        uint32_t value; /* offset into the language arena */
        uint32_t hash;  /* cached hash of the key, to skip most strcmp calls */
        uint32_t next;  /* next entry in this bucket's chain, or CI18N_NO_INDEX */
    } ci18n_entry_t;

    typedef struct ci18n_language
    {
        char code[CI18N_MAX_CODE_LENGTH];
        ci18n_arena_t strings;
        ci18n_entry_t *entries; /* dense, in insertion order */
        uint32_t *buckets;      /* hash bucket to entry index */
        size_t count;
        size_t capacity;
        size_t bucket_count; /* always a power of two, or zero */
    } ci18n_language_t;

    /*
     * Why the last call failed. Every function that can fail sets this, and
     * every one that succeeds clears it to CI18N_OK, so read it right after
     * the call you care about.
     */
    typedef enum ci18n_error
    {
        CI18N_OK = 0,
        CI18N_ERR_NOT_INITIALIZED,   /* ci18n_init() has not been called */
        CI18N_ERR_INVALID_ARGUMENT,  /* a NULL or otherwise unusable argument */
        CI18N_ERR_CODE_TOO_LONG,     /* language code exceeds CI18N_MAX_CODE_LENGTH */
        CI18N_ERR_FILE_NOT_FOUND,    /* the file could not be opened */
        CI18N_ERR_OUT_OF_MEMORY,     /* an allocation failed */
        CI18N_ERR_TOO_MANY_LANGUAGES,/* CI18N_MAX_LANGUAGES reached */
        CI18N_ERR_TOO_MANY_KEYS,     /* CI18N_MAX_KEYS_PER_LANGUAGE reached */
        CI18N_ERR_LANGUAGE_NOT_FOUND,/* no such language is loaded */
        CI18N_ERR_KEY_NOT_FOUND,     /* no such key in the languages consulted */
        CI18N_ERR_PARSE,             /* the load dropped or truncated something */
        CI18N_ERR_TOO_MANY_FORMATTERS,/* CI18N_MAX_FORMATTERS reached */
        CI18N_ERR_UNKNOWN_FORMATTER  /* a translation asked for one that is not registered */
    } ci18n_error_t;

    /*
     * The highest error code, so code that walks them all needs no edit when
     * one is added. A macro rather than a trailing enumerator on purpose: an
     * enumerator would have to be handled by every exhaustive switch over
     * ci18n_error_t, forever, despite not being an error.
     */
#define CI18N_ERR_LAST CI18N_ERR_UNKNOWN_FORMATTER

    /*
     * What the last load actually did. A loader returns true whenever it could
     * read the source, which says nothing about the contents, so this is where
     * you find out that half the file was silently dropped.
     */
    typedef struct ci18n_load_stats
    {
        size_t lines_read;           /* lines the loader looked at */
        size_t entries_loaded;       /* entries added or updated */
        size_t lines_skipped;        /* comments and blank lines */
        size_t lines_malformed;      /* no separator, or an empty key */
        size_t first_malformed_line; /* 1-based, 0 when there were none */
        size_t keys_truncated;       /* keys cut to CI18N_MAX_KEY_LENGTH */
        size_t values_truncated;     /* values cut to CI18N_MAX_VALUE_LENGTH */
        size_t lines_truncated;      /* lines longer than CI18N_MAX_LINE_LENGTH */
    } ci18n_load_stats_t;

    /*
     * A function you supply that renders one value.
     *
     * Follows snprintf(): write at most capacity-1 bytes, terminate whenever
     * capacity is non-zero, and return the length the whole result would have
     * had. The library calls with out = NULL and capacity = 0 to measure, so
     * handle that without writing.
     *
     * `value` is the string the caller passed for this placeholder, and `arg`
     * is whatever followed the comma in the translation, or "" when there was
     * none. Neither is ever NULL.
     *
     * `user_data` is the pointer given at registration, untouched.
     *
     * In the shared threading mode this runs while the catalogue's read lock
     * is held, so it must not call back into the library, and it must be safe
     * to run on several threads at once.
     */
    typedef size_t (*ci18n_formatter_fn)(char *out, size_t capacity,
                                         const char *value, const char *arg,
                                         void *user_data);

    /*
     * Called once per entry by ci18n_foreach(). Return true to go on, false
     * to stop early.
     *
     * Both strings belong to the library and live only for the call, so copy
     * what you want to keep. The callback runs while the catalogue's read
     * lock is held in the shared threading mode, so it must not call back
     * into the library.
     */
    typedef bool (*ci18n_entry_fn)(const char *key, const char *value,
                                   void *user_data);

    typedef struct ci18n_formatter
    {
        char name[CI18N_MAX_FORMATTER_NAME];
        ci18n_formatter_fn fn;
        void *user_data;
    } ci18n_formatter_t;

    typedef struct ci18n_context
    {
        ci18n_language_t languages[CI18N_MAX_LANGUAGES];
        size_t language_count;
        char current_language[CI18N_MAX_CODE_LENGTH];
        char fallback_language[CI18N_MAX_CODE_LENGTH];
        ci18n_error_t last_error;
        ci18n_load_stats_t load_stats;
#ifndef CI18N_NO_FORMAT
        ci18n_formatter_t formatters[CI18N_MAX_FORMATTERS];
        size_t formatter_count;
        bool bidi_isolation; /* wrap filled-in values in FSI ... PDI */
#endif
        bool initialized;
#ifdef CI18N_THREAD_SHARED
        /* Inside the catalogue, not beside the global, so that every
         * catalogue locks itself. */
        ci18n_lock_shard_t locks[CI18N_LOCK_SHARDS];
#endif
    } ci18n_context_t;

    /*
     * A catalogue: a set of languages with its own current and fallback
     * selection.
     *
     * Every function has a variant taking one of these, and the plain names
     * are those variants applied to a default catalogue. That is the whole
     * difference, and it matters when ci18n is used inside a library: without
     * it, the library and the application that linked it would fight over one
     * selection, and whoever called ci18n_set_current() last would win.
     *
     * The struct is visible because ci18n_get_context() hands it out, not
     * because its layout is part of the API. Use the functions.
     */
    typedef ci18n_context_t ci18n_t;

    /* ============================================================================
     * Pointer lifetime
     * ============================================================================
     *
     * Every `const char *` this API returns points into library storage, not
     * into a copy you own. Such a pointer stays valid only until the next call
     * that mutates the language it came from:
     *
     *   ci18n_set(), ci18n_load_language() and ci18n_load_from_buffer() may
     *   grow the string arena, which moves every key and value of that
     *   language at once, not only the one being written
     *
     *   ci18n_clear(), ci18n_free() and ci18n_set_current() invalidate
     *   pointers outright
     *
     *   ci18n_remove() leaves the arena alone, so other pointers survive it,
     *   but do not rely on that: it is an implementation detail
     *
     * So this is a use-after-free:
     *
     *   const char *greeting = ci18n_get("greeting");
     *   ci18n_load_language("en", "extra.txt");   // may realloc
     *   puts(greeting);                           // dangling
     *
     * Read a translation right before you use it, which is cheap, or copy it
     * if you need to hold it:
     *
     *   char greeting[64];
     *   snprintf(greeting, sizeof(greeting), "%s", ci18n_get_or_key("greeting"));
     *
     * Note that ci18n_get_or_key() can also hand back the `key` pointer you
     * passed in, so the lifetime is then your string literal's, not ours.
     *
     * ============================================================================
     * Public API
     * ============================================================================ */

    /* Initialize the i18n system. Must be called before any other function. */
    CI18N_DEF bool ci18n_init(void);

    /* Shutdown and free all resources. */
    CI18N_DEF void ci18n_free(void);

#if !defined(CI18N_NO_FILES)
    /*
     * Load translations from a file.
     *
     * File format: key=value, one per line, # or ; for comments. Lines that
     * carry no separator are skipped. Merges into the language if it already
     * exists, overwriting the keys it repeats.
     *
     * Success means the file was opened and read, not that its contents were
     * valid: see ci18n_last_load_stats() for what was dropped or truncated.
     *
     * Returns: true if the file was read, false on failure
     */
    CI18N_DEF bool ci18n_load_language(const char *language_code, const char *filepath);
#endif

    /*
     * Load translations from a memory buffer.
     *
     * Same format and merge behaviour as ci18n_load_language(), and the same
     * note about what success means. `length` is in bytes and the buffer need
     * not be NUL terminated.
     *
     * Returns: true if the buffer was read, false on failure
     */
    CI18N_DEF bool ci18n_load_from_buffer(const char *language_code, const char *buffer, size_t length);

#ifndef CI18N_NO_MO
#if !defined(CI18N_NO_FILES)
    /*
     * Load a compiled gettext catalogue, a .mo file, into a language.
     *
     * For a project that already has gettext translations: keep the .po
     * files and the tools around them, and load what msgfmt produces. No
     * libintl, no setlocale, no conversion step.
     *
     *   ci18n_load_mo("ru", "locale/ru/LC_MESSAGES/app.mo");
     *   ci18n_get("Hello, world");            // keys are the msgids
     *
     * How entries map, the same way tools/po2ci18n.py maps them:
     *
     *   msgid "Open"                     key "Open"
     *   msgctxt "menu" + msgid "Open"    key "menu.Open"
     *   msgid_plural, msgstr[0..n]       keys "msgid[one]", "msgid[few]", ...
     *
     * Plural indexes become CLDR categories by the header's nplurals: 2 is
     * one, other; 3 is one, few, many; and so on for the shapes gettext
     * catalogues use. The header entry itself is skipped, and msgfmt has
     * already dropped fuzzy and untranslated entries.
     *
     * Both byte orders are read. Every offset is checked against the size
     * before use, so a truncated or hostile file is refused, never read past.
     * In the load stats a "line" is an entry, numbered from 1.
     *
     * Define CI18N_NO_MO to leave this out of the build.
     *
     * Returns: false if the file cannot be read or is not a .mo file
     * (CI18N_ERR_PARSE), true otherwise; see ci18n_last_load_stats()
     */
    CI18N_DEF bool ci18n_load_mo(const char *language_code, const char *filepath);
#endif

    /* The same, from bytes already in memory, such as a catalogue embedded in
     * the binary. The buffer is only read during the call. */
    CI18N_DEF bool ci18n_load_mo_from_buffer(const char *language_code, const void *data, size_t length);
#endif

    /*
     * Set the current active language.
     * Returns: true if language exists, false otherwise
     */
    CI18N_DEF bool ci18n_set_current(const char *language_code);

    /*
     * Set the fallback language (used when key not found in current language).
     * Returns: true if language exists, false otherwise
     */
    CI18N_DEF bool ci18n_set_fallback(const char *language_code);

    /*
     * Get translation for a key in the current language, falling back to the
     * fallback language if set.
     *
     * The result points into library storage. See "Pointer lifetime" above.
     *
     * Returns: translation string or NULL if not found
     */
    CI18N_DEF const char *ci18n_get(const char *key);

    /*
     * Get translation with fallback to the key itself if not found.
     *
     * The result points either into library storage or at the `key` you
     * passed in. See "Pointer lifetime" above.
     *
     * Returns: translation string or the key if not found
     */
    CI18N_DEF const char *ci18n_get_or_key(const char *key);

    /*
     * Copy a translation into a buffer you own.
     *
     * The safe read under CI18N_THREAD_SHARED: the copy is made while the
     * read lock is still held, so it cannot be invalidated by a writer the
     * way a returned pointer can. Useful single-threaded too, whenever a
     * translation has to outlive the next ci18n_set() or load.
     *
     * Follows snprintf(): at most capacity-1 bytes are written, the result is
     * terminated when capacity is non-zero, and the return value is the full
     * length. Pass out = NULL with capacity = 0 to measure.
     *
     * Returns: the length of the translation, or 0 if the key was not found
     */
    CI18N_DEF size_t ci18n_get_copy(const char *key, char *out, size_t capacity);

    /*
     * Check if a key exists in current language.
     * Returns: true if exists, false otherwise
     */
    CI18N_DEF bool ci18n_has(const char *key);

    /*
     * Get the current language code.
     *
     * The result points into library storage and is invalidated by
     * ci18n_set_current() and ci18n_free().
     *
     * Returns: language code string or empty string if not set
     */
    CI18N_DEF const char *ci18n_get_current(void);

    /*
     * List the loaded language codes into a buffer you own.
     *
     * Writes at most `capacity` codes into `out` and returns how many
     * languages are loaded in total, which may exceed `capacity`. Pass
     * out = NULL to ask for that total without writing anything:
     *
     *   const char *codes[8];
     *   size_t total = ci18n_get_languages(codes, 8);
     *   size_t n = total < 8 ? total : 8;
     *   for (size_t i = 0; i < n; i++) puts(codes[i]);
     *
     * The codes point into library storage. See "Pointer lifetime" above.
     *
     * Returns: total number of loaded languages
     */
    CI18N_DEF size_t ci18n_get_languages(const char **out, size_t capacity);

    /*
     * Add a translation entry programmatically, creating the language if
     * needed. Overwrites the key if it already exists.
     *
     * Returns: true on success, false on failure
     */
    CI18N_DEF bool ci18n_set(const char *language_code, const char *key, const char *value);

    /*
     * Remove a translation entry.
     * Returns: true if removed, false if not found
     */
    CI18N_DEF bool ci18n_remove(const char *language_code, const char *key);

    /*
     * Clear all translations for a language.
     * Returns: true if cleared, false if language not found
     */
    CI18N_DEF bool ci18n_clear(const char *language_code);

    /*
     * Unload a language entirely, freeing its memory and releasing its slot.
     *
     * ci18n_clear() empties a language but keeps it loaded, so it still
     * occupies one of CI18N_MAX_LANGUAGES. This removes it outright, which is
     * what you want for a program that switches between many languages over a
     * long run and would otherwise fill the table with empty ones.
     *
     * If the language being removed is the current or the fallback one, that
     * selection is cleared, since pointing at a language that no longer exists
     * would be worse than pointing at nothing.
     *
     * Returns: true if removed, false if no such language
     */
    CI18N_DEF bool ci18n_remove_language(const char *language_code);

    /*
     * Get translation count for a language.
     * Returns: number of entries, or 0 if language not found
     */
    CI18N_DEF size_t ci18n_count(const char *language_code);

    /*
     * Visit every entry of one language, in no particular order.
     *
     * This is how a tool finds keys it did not know about: a checker that
     * compares two languages, an exporter, a dump for debugging. Plural forms
     * show up as their stored keys, `files[one]` and so on.
     *
     * Returns: how many entries the callback saw, counting the one that
     * stopped the walk; 0 if the language is not loaded
     */
    CI18N_DEF size_t ci18n_foreach(const char *language_code, ci18n_entry_fn fn,
                                   void *user_data);

    /*
     * Check if the system is initialized.
     * Returns: true if initialized, false otherwise
     */
    CI18N_DEF bool ci18n_is_initialized(void);

    /* ============================================================================
     * Plurals
     * ============================================================================
     *
     * Plural forms are ordinary keys with the category in brackets, so the file
     * format and the parser are unchanged:
     *
     *   files[one]=%d file
     *   files[other]=%d files
     *
     * Russian needs three, and this is why a key-value table alone cannot
     * translate it:
     *
     *   files[one]=%d файл
     *   files[few]=%d файла
     *   files[many]=%d файлов
     *
     * Then ask for a count rather than a key:
     *
     *   printf(ci18n_plural_or_key("files", n), n);
     *
     * Which categories a language uses is decided by CLDR, not by you. See
     * ci18n_plural_category() for which languages are known and what an
     * unknown one falls back to.
     * ============================================================================ */

    /*
     * The CLDR plural categories. Which of them a language actually uses
     * varies: English has one and other, Russian has one, few and many,
     * Japanese has only other, Arabic uses all six.
     */
    typedef enum ci18n_plural_category
    {
        CI18N_PLURAL_ZERO = 0,
        CI18N_PLURAL_ONE,
        CI18N_PLURAL_TWO,
        CI18N_PLURAL_FEW,
        CI18N_PLURAL_MANY,
        CI18N_PLURAL_OTHER
    } ci18n_plural_category_t;

    /*
     * Which category `count` falls into for a language.
     *
     * Rules come from CLDR and are grouped by family, since most languages
     * share one. Known families cover: English-like one/other, French and
     * Portuguese where zero is also "one", the Romance "many" for a whole
     * number of millions, Russian, Ukrainian and Belarusian,
     * Polish, Czech and Slovak, Croatian and Serbian, Arabic, Hebrew with its
     * dual, Lithuanian, Latvian, Slovenian, Irish, Romanian, and the
     * languages with no plural distinction at all such as Japanese, Chinese
     * and Korean. Every supported language is checked against CLDR 48 for
     * every count from 0 to 10000.
     *
     * Matching uses the primary subtag, so "ru-RU" and "ru_RU.UTF-8" both
     * resolve as Russian. An unknown language is treated as English-like,
     * which is the least surprising guess: one for exactly 1, other for
     * everything else.
     *
     * Only integer counts are considered. CLDR distinguishes 1 from 1.0 in
     * some languages; this does not.
     *
     * Returns: the category for that count
     */
    CI18N_DEF ci18n_plural_category_t ci18n_plural_category(const char *language_code,
                                                            long count);

    /*
     * The category's CLDR name, which is also the bracket suffix to use in a
     * translation file: "zero", "one", "two", "few", "many" or "other".
     *
     * Returns: a static string, never NULL
     */
    CI18N_DEF const char *ci18n_plural_category_name(ci18n_plural_category_t category);

    /*
     * Get the plural form of a key for `count`, in the current language.
     *
     * Tries three keys in order, so a translation only has to be as detailed
     * as it needs to be:
     *
     *   key[<category>]   the right form for this count
     *   key[other]        the catch-all form
     *   key               a translation with no plural forms at all
     *
     * The result points into library storage. See "Pointer lifetime" above.
     *
     * Returns: translation string or NULL if none of the three exist
     */
    CI18N_DEF const char *ci18n_plural(const char *key, long count);

    /*
     * Same, falling back to the key itself rather than NULL.
     *
     * Returns: translation string, or the key if nothing was found
     */
    CI18N_DEF const char *ci18n_plural_or_key(const char *key, long count);

#if !defined(CI18N_NO_ORDINALS)
    /*
     * Which category `count` falls into as an ordinal: first, second, third,
     * rather than one, two, three.
     *
     * Ordinals use the same category names as cardinals but different rules.
     * English has four forms (1st, 2nd, 3rd, 4th), and 11, 12 and 13 take the
     * fourth despite ending in 1, 2 and 3. Russian, German, Spanish, Polish
     * and most others have only "other", because their ordinal is one word
     * that agrees with its noun rather than one of a few suffixes.
     *
     * Rules come from CLDR 48 and are grouped by family. An unknown language
     * is treated as having no ordinal distinction, which is the commonest
     * case and the safe one: "other" is a form every translation has.
     *
     * Returns: the category for that count
     */
    CI18N_DEF ci18n_plural_category_t ci18n_ordinal_category(const char *language_code,
                                                             long count);

    /*
     * Get the ordinal form of a key for `count`, in the current language.
     *
     * Same three-step lookup as ci18n_plural(), and the same bracket
     * suffixes, so an ordinal key is just a key used for ordinals:
     *
     *   place[one]={count}st place
     *   place[two]={count}nd place
     *   place[few]={count}rd place
     *   place[other]={count}th place
     *
     * Keep ordinal and cardinal forms under different keys. "files[one]" and
     * "place[one]" mean different things, and a key cannot be both.
     *
     * Returns: translation string or NULL if none of the three exist
     */
    CI18N_DEF const char *ci18n_ordinal(const char *key, long count);

    /*
     * Same, falling back to the key itself rather than NULL.
     *
     * Returns: translation string, or the key if nothing was found
     */
    CI18N_DEF const char *ci18n_ordinal_or_key(const char *key, long count);
#endif

    /* ============================================================================
     * Text direction
     * ============================================================================
     *
     * Arabic, Hebrew, Persian and a few dozen other languages are written
     * right to left, and an interface has to know which way round to put
     * things. The direction is a property of the language, so the library can
     * answer it, while the layout itself is yours to do.
     *
     * The name is the one HTML and CSS use, so it drops straight in:
     *
     *   printf("<html dir=\"%s\">", ci18n_direction_name(ci18n_current_direction()));
     *
     * This is the direction of a language, not of a sentence. For a value in
     * the other direction inside a translation, an English name in Arabic,
     * see ci18n_set_bidi_isolation() below and docs/unicode-and-direction.md.
     * ============================================================================ */

    /*
     * Which way a language is written.
     *
     * The values match the CSS `direction` property and the HTML `dir`
     * attribute, and CI18N_DIR_LTR is zero so a zeroed struct means left to
     * right.
     */
    typedef enum ci18n_direction
    {
        CI18N_DIR_LTR = 0,
        CI18N_DIR_RTL
    } ci18n_direction_t;

    /*
     * The direction of a language code.
     *
     * A script subtag decides on its own, because direction is a property of
     * the script rather than the language: "az-Arab" is right to left even
     * though "az" is not, and romanized "ar-Latn" is left to right even
     * though Arabic is not.
     *
     * Without a script subtag the primary subtag is looked up in a table of
     * the languages whose default script is right to left, so "ar", "ar-EG"
     * and "ar_EG.UTF-8" all resolve the same way. Matching ignores case, as
     * BCP 47 says it should.
     *
     * An unknown language is left to right. That is the commoner answer, and
     * the safer one: a left-to-right interface shown right to left is broken
     * in a way nobody misses, while the reverse merely looks untranslated.
     *
     * Returns: CI18N_DIR_RTL or CI18N_DIR_LTR, and CI18N_DIR_LTR for NULL
     */
    CI18N_DEF ci18n_direction_t ci18n_direction(const char *language_code);

    /*
     * The direction's name, "ltr" or "rtl".
     *
     * These are the values HTML's dir attribute and CSS's direction property
     * take, which is what this is for.
     *
     * Returns: a static string, never NULL
     */
    CI18N_DEF const char *ci18n_direction_name(ci18n_direction_t direction);

    /*
     * The direction of the current language.
     *
     * Shorthand for ci18n_direction(ci18n_get_current()), which is what you
     * want almost every time. Left to right when no language is set.
     *
     * Returns: CI18N_DIR_RTL or CI18N_DIR_LTR
     */
    CI18N_DEF ci18n_direction_t ci18n_current_direction(void);

    /*
     * Unicode bidi controls, as UTF-8 string literals.
     *
     * Mixed-direction text goes wrong in a way that is easy to miss: an
     * English name inside an Arabic sentence drags the punctuation after it
     * to the wrong side, and a number after a Hebrew word can swap places
     * with it. Isolating the inserted part fixes that: FSI opens an isolate
     * whose direction comes from its own first strong character, PDI closes
     * it. LRM and RLM are invisible characters with a fixed direction, for
     * pinning down the end of a line that finishes in a digit or a symbol.
     *
     * Invisible when rendered, but they are real characters: 3 bytes each,
     * and they count in ci18n_utf8_length().
     */
#define CI18N_FSI "\xE2\x81\xA8" /* U+2068 FIRST STRONG ISOLATE */
#define CI18N_PDI "\xE2\x81\xA9" /* U+2069 POP DIRECTIONAL ISOLATE */
#define CI18N_LRM "\xE2\x80\x8E" /* U+200E LEFT-TO-RIGHT MARK */
#define CI18N_RLM "\xE2\x80\x8F" /* U+200F RIGHT-TO-LEFT MARK */

#if !defined(CI18N_NO_FORMAT)
    /*
     * Isolate every value ci18n_format() and its relatives fill in.
     *
     * With this on, "{name}" expands to FSI, the value, PDI, so a value
     * written in the other direction cannot reorder the sentence around it.
     * Turn it on for anything shown to people in a right-to-left language,
     * or that shows right-to-left names or text in a left-to-right one.
     * Leave it off for logs, file names and anything a program parses.
     *
     * Off by default, and per catalogue. ci18n_free() turns it off again.
     *
     * Returns: false only before ci18n_init()
     */
    CI18N_DEF bool ci18n_set_bidi_isolation(bool enabled);
#endif

    /*
     * Copy text into out wrapped in FSI ... PDI, for a value you place
     * yourself rather than through ci18n_format().
     *
     * Follows snprintf(): writes at most capacity-1 bytes, always
     * terminates when capacity is non-zero, never cuts a character in half,
     * and returns the length the whole result would have had. Pass
     * out = NULL, capacity = 0 to measure.
     *
     * Returns: the full length, 6 bytes more than text; 0 for NULL text
     */
    CI18N_DEF size_t ci18n_bidi_isolate(char *out, size_t capacity, const char *text);

    /*
     * The mark for a direction: CI18N_RLM for right to left, CI18N_LRM
     * otherwise. Append it after a trailing number or symbol so the line
     * ends the way its language reads.
     *
     * Returns: a static string, never NULL
     */
    CI18N_DEF const char *ci18n_bidi_mark(ci18n_direction_t direction);

    /* ============================================================================
     * Numbers
     * ============================================================================
     *
     * "1234567.5" is written 1,234,567.5 in English, 1 234 567,5 in Russian,
     * 1.234.567,5 in German and 12,34,567.5 in Hindi. The library knows the
     * separators, the minus sign and the grouping of every language it has
     * plural rules for, taken from CLDR by tools/cldr_numbers.py.
     *
     * In a translation, name the built-in formatter:
     *
     *   total=Total: {n:number}          any fraction digits the value has
     *   price=Price: {n:number,2}        exactly two, rounded half to even
     *
     *   ci18n_format(out, sizeof(out), "total", "n", "1234567.5", NULL);
     *
     * The number is a string, "-1234.5", so nothing is lost to a double on
     * the way; print yours with "%ld" or "%.2f" first. A value that is not a
     * plain decimal number comes out unchanged. The language is the current
     * one. A formatter you register under the name "number" replaces this.
     *
     * Digits stay 0 to 9 in every language. Currency, percent, compact forms
     * like "1.2K" and native digits are not here.
     */

#if !defined(CI18N_NO_NUMBERS)
    /*
     * Write `number` the way `language_code` writes it.
     *
     * fraction_digits of -1 keeps the fraction as given; 0 to 20 rounds or
     * pads to exactly that many, rounding half to even as CLDR does.
     * Follows snprintf(): pass out = NULL, capacity = 0 to measure.
     *
     * Returns: the full length of the result
     */
    CI18N_DEF size_t ci18n_format_number(char *out, size_t capacity,
                                         const char *language_code,
                                         const char *number, int fraction_digits);
#endif

    /* ============================================================================
     * Interpolation
     * ============================================================================
     *
     * Translations need values dropped into them, and the order those values
     * appear in differs between languages, which is exactly what positional
     * formatting cannot express. So placeholders are named:
     *
     *   greeting=Hello, {name}! You have {count} messages.
     *   greeting=Привет, {name}! У вас {count} сообщений.
     *
     *   char text[256];
     *   ci18n_format(text, sizeof(text), "greeting",
     *                "name", "Ilya", "count", "3", NULL);
     *
     * A translator can move {name} and {count} around freely, or use one
     * twice, or leave one out. Write {{ and }} for literal braces.
     *
     * Values are strings, not a format string: a translation file is data,
     * often not written by you, and handing it to printf() as a format makes
     * every translator a potential attacker. Convert numbers yourself, or use
     * ci18n_format_plural() which does it for the count.
     * ============================================================================ */

#if !defined(CI18N_NO_FORMAT)
    /*
     * Fill a translation's placeholders from name and value pairs.
     *
     * Arguments after `key` are `const char *` pairs, terminated by NULL:
     *
     *   ci18n_format(out, sizeof(out), "greeting", "name", user, NULL);
     *
     * Follows snprintf(): at most capacity-1 bytes are written, the result is
     * always terminated when capacity is non-zero, and the return value is
     * the length the whole result would have had. A return of capacity or
     * more means it was truncated. Passing out = NULL with capacity = 0
     * measures without writing, which is how you size a buffer.
     *
     * Truncation stops at a character boundary, so a cut result is still
     * valid UTF-8. That means it can come out shorter than capacity-1: a
     * 12-byte Russian greeting cut into 8 bytes gives 6 bytes rather than 7,
     * because the seventh would have been half of a character.
     *
     * A placeholder with no matching name is left exactly as it appears, so a
     * typo in a translation shows up in the output instead of vanishing.
     *
     * Returns: the full length of the result, not counting the terminator,
     * or 0 if the key was not found
     */
    CI18N_DEF size_t ci18n_format(char *out, size_t capacity, const char *key, ...);

    /*
     * Pick the plural form for `count`, then fill its placeholders.
     *
     * The count is available as {count} without being passed as a pair,
     * since a plural sentence almost always mentions the number it is about:
     *
     *   files[one]={count} файл
     *   files[few]={count} файла
     *   files[many]={count} файлов
     *
     *   ci18n_format_plural(out, sizeof(out), "files", n, NULL);
     *
     * Any other placeholders come from name and value pairs as usual, and a
     * pair named "count" overrides the number.
     *
     * Returns: as ci18n_format()
     */
    CI18N_DEF size_t ci18n_format_plural(char *out, size_t capacity, const char *key,
                                         long count, ...);
#endif

#if !defined(CI18N_NO_FORMAT) && !defined(CI18N_NO_ORDINALS)
    /*
     * Pick the ordinal form for `count`, then fill its placeholders.
     *
     * The ordinal counterpart of ci18n_format_plural(), with {count}
     * available in the same way:
     *
     *   place[one]={count}st place
     *   place[other]={count}th place
     *
     *   ci18n_format_ordinal(out, sizeof(out), "place", 21, NULL);
     *   (gives "21st place")
     *
     * Returns: as ci18n_format()
     */
    CI18N_DEF size_t ci18n_format_ordinal(char *out, size_t capacity, const char *key,
                                          long count, ...);
#endif

    /* ============================================================================
     * UTF-8
     * ============================================================================
     *
     * The library is byte-transparent: it stores and returns whatever you
     * give it, so UTF-8 passes through untouched and these helpers are here
     * for the places where bytes are not enough.
     *
     * Two things bite in practice. Counting characters is not counting bytes,
     * so a field that allows twenty characters cannot be checked with
     * strlen(). And cutting a string to fit a buffer can land in the middle
     * of a character, which produces bytes no decoder will accept. The
     * library's own formatting handles the second case for you; these let you
     * handle it in your own code.
     *
     *   char label[16];
     *   ci18n_get_copy("title", label, sizeof(label));
     *   ci18n_utf8_truncate(label, sizeof(label) - 1);
     *
     * Validation is strict, in the sense the Unicode standard requires:
     * overlong encodings, surrogate halves and anything above U+10FFFF are
     * rejected rather than tolerated. Lenient decoders are how mismatched
     * validation turns into a security bug.
     * ============================================================================ */

    /*
     * Whether `text` is well-formed UTF-8.
     *
     * Rejects what the standard says is not UTF-8, not merely what fails to
     * decode: a character encoded in more bytes than it needs (overlong), a
     * surrogate half in the range U+D800 to U+DFFF, a value above U+10FFFF,
     * a continuation byte where a lead byte belongs, and a sequence cut short
     * by the end of the string.
     *
     * Note that an empty string is valid, and so is any pure ASCII string.
     *
     * Returns: true if well-formed, false otherwise, and false for NULL
     */
    CI18N_DEF bool ci18n_utf8_valid(const char *text);

    /*
     * How many characters `text` holds, rather than how many bytes.
     *
     * "Character" here means one Unicode codepoint. That is the useful answer
     * for a length limit, though it is still not the number of things a
     * reader would count: an accent written as a separate combining mark is
     * its own codepoint, and an emoji can be several. Counting those needs
     * grapheme clusters, which is a CLDR-sized job and not here.
     *
     * Invalid bytes count as one character each, so the answer is always
     * defined and never exceeds the byte length. Check with
     * ci18n_utf8_valid() when you need to know the input was sound.
     *
     * Returns: the number of codepoints, and 0 for NULL
     */
    CI18N_DEF size_t ci18n_utf8_length(const char *text);

    /*
     * How many bytes the character at `text` occupies.
     *
     * Use it to step through a string one character at a time:
     *
     *   for (const char *p = text; *p; p += ci18n_utf8_sequence_length(p))
     *       ...
     *
     * A return of 0 would make that loop spin, so it never happens for a
     * non-empty string: an invalid byte reports 1, which steps past the
     * problem rather than stalling on it.
     *
     * Returns: 1 to 4 for a valid character, 1 for an invalid byte, and 0 at
     * the terminator or for NULL
     */
    CI18N_DEF size_t ci18n_utf8_sequence_length(const char *text);

    /*
     * Shorten `text` in place so it fits `max_bytes`, cutting only at a
     * character boundary.
     *
     * `max_bytes` is the budget for the text itself, with the terminator not
     * counted, so a `char buf[16]` takes `sizeof(buf) - 1`. Text already
     * within budget is left alone.
     *
     * The result can be shorter than the budget, by up to three bytes, since
     * the cut moves back to the last boundary rather than landing mid
     * character. Nothing is appended: if you want an ellipsis, there is room
     * for one because you chose the budget.
     *
     * Returns: the new length in bytes, and 0 for NULL
     */
    CI18N_DEF size_t ci18n_utf8_truncate(char *text, size_t max_bytes);

    /* ============================================================================
     * Formatters
     * ============================================================================
     *
     * A date in a sentence is a translation problem twice over: where it goes
     * is the translator's business, and how it reads is the locale's. The
     * first half is already solved by named placeholders. The second half is
     * not something this library will ever know, because knowing it means
     * shipping CLDR.
     *
     * So the translation names a formatter and your code provides it:
     *
     *   invoice=Issued {created:date,long}, due {due:date,short}
     *
     *   static size_t format_date(char *out, size_t capacity,
     *                             const char *value, const char *arg,
     *                             void *user_data)
     *   {
     *       (void)user_data;
     *       return my_render_date(out, capacity, value, arg);
     *   }
     *
     *   ci18n_set_formatter("date", format_date, NULL);
     *   ci18n_format(text, sizeof(text), "invoice",
     *                "created", "2026-09-23", "due", "2026-10-07", NULL);
     *
     * The library does the parsing, the lookup and the buffer arithmetic; you
     * do the rendering, with whatever library you already use for dates. A
     * translator can move the placeholder, change which form is asked for, or
     * drop it, without touching your code.
     *
     * Placeholder syntax is {name:formatter} or {name:formatter,argument}.
     * Everything after the first comma is the argument, verbatim, so a
     * formatter can define its own syntax there. Without a colon a
     * placeholder behaves exactly as it always has.
     *
     * Formatters belong to a catalogue and are not inherited from the default
     * one. A library using its own catalogue registers its own, which is the
     * point of having catalogues; it also means formatting never has to take
     * two locks at once.
     * ============================================================================ */

#if !defined(CI18N_NO_FORMAT)
    /*
     * Register `fn` under `name`, replacing any formatter already there.
     *
     * `user_data` is handed back to the formatter on every call and is never
     * inspected, so it can carry your locale object, your arena, or nothing.
     *
     * Register before formatting, normally once at startup. In the shared
     * threading mode this takes the write lock, so it is safe to call later,
     * but a formatter that appears halfway through a run is a confusing thing
     * to debug.
     *
     * Returns: true on success, false if the name is empty, too long for
     * CI18N_MAX_FORMATTER_NAME, `fn` is NULL, or CI18N_MAX_FORMATTERS is full
     */
    CI18N_DEF bool ci18n_set_formatter(const char *name, ci18n_formatter_fn fn,
                                       void *user_data);

    /*
     * Forget the formatter called `name`.
     *
     * Translations asking for it then leave their placeholder visible and
     * report CI18N_ERR_UNKNOWN_FORMATTER, the same as one that was never
     * registered.
     *
     * Returns: true if one was removed, false if there was no such formatter
     */
    CI18N_DEF bool ci18n_remove_formatter(const char *name);
#endif

    /* ============================================================================
     * Locale detection
     * ============================================================================ */

#if !defined(CI18N_NO_LOCALE)
    /*
     * The user's locale, as the environment reports it.
     *
     * Looks at LC_ALL, then LC_MESSAGES, then LANG. On Windows, where those
     * are usually unset, it asks the system for the user's default locale
     * name instead. Define CI18N_NO_PLATFORM_LOCALE to skip that and keep
     * windows.h out of your build, leaving only the environment variables.
     *
     * The result is normalised: the encoding and any modifier are dropped and
     * underscores become hyphens, so "ru_RU.UTF-8" arrives as "ru-RU". The
     * "C" and "POSIX" locales report nothing, since they name no language.
     *
     * Returns: the length written, or 0 when nothing could be determined
     */
    CI18N_DEF size_t ci18n_detect_locale(char *out, size_t capacity);

    /*
     * Select the best loaded language for a locale, trying less specific
     * forms as it goes.
     *
     * For "ru-RU" that is "ru-RU", then "ru". So a program can load plain
     * "ru" and still honour a user asking for Russian as spoken in Russia,
     * which is what makes this worth having over ci18n_set_current().
     *
     * Pass NULL to use the locale from ci18n_detect_locale().
     *
     * The current language is left alone if nothing matches, so a failed call
     * cannot leave the program with no language at all.
     *
     * Returns: true if a language was selected, false if none matched
     */
    CI18N_DEF bool ci18n_set_current_best(const char *locale);
#endif

    /* ============================================================================
     * Catalogues
     * ============================================================================
     *
     * Everything above works on one catalogue the library owns. That is
     * convenient for a program, and wrong for a library: if ci18n is used
     * inside a reusable component, the component and the application that
     * linked it share one current language, and whichever called
     * ci18n_set_current() last wins.
     *
     * So every function that touches a catalogue has an _in variant taking
     * one explicitly, and the plain names are those variants applied to the
     * default catalogue. Nothing else differs.
     *
     *   ci18n_t *ui = ci18n_create();
     *   ci18n_load_language_in(ui, "en", "en.txt");
     *   ci18n_set_current_in(ui, "en");
     *   puts(ci18n_get_or_key_in(ui, "greeting"));
     *   ci18n_destroy(ui);
     *
     * A catalogue carries its own languages, its own current and fallback
     * selection, and in the shared threading mode its own lock, so two of
     * them never wait on each other.
     * ============================================================================ */

    /*
     * Create a catalogue, ready to load into.
     *
     * Returns: a catalogue, or NULL if it could not be allocated
     */
    CI18N_DEF ci18n_t *ci18n_create(void);

    /*
     * Free a catalogue and everything it holds.
     *
     * Passing NULL is a no-op, so this can be called on a failed create.
     * Never pass the default catalogue: it is not one of these.
     */
    CI18N_DEF void ci18n_destroy(ci18n_t *catalog);

    /*
     * The default catalogue, the one the plain functions use. Handy for code
     * written against the _in functions that wants to keep using it.
     *
     * Returns: the default catalogue, never NULL
     */
    CI18N_DEF ci18n_t *ci18n_default(void);

#if !defined(CI18N_NO_FILES)
    /* The _in variants. Each behaves exactly as the function it is named
     * after, on the catalogue given rather than on the default one. */
    CI18N_DEF bool ci18n_load_language_in(ci18n_t *catalog, const char *language_code, const char *filepath);
#endif
    CI18N_DEF bool ci18n_load_from_buffer_in(ci18n_t *catalog, const char *language_code, const char *buffer, size_t length);
#ifndef CI18N_NO_MO
#if !defined(CI18N_NO_FILES)
    CI18N_DEF bool ci18n_load_mo_in(ci18n_t *catalog, const char *language_code, const char *filepath);
#endif
    CI18N_DEF bool ci18n_load_mo_from_buffer_in(ci18n_t *catalog, const char *language_code,
                                                const void *data, size_t length);
#endif
    CI18N_DEF bool ci18n_set_current_in(ci18n_t *catalog, const char *language_code);
    CI18N_DEF bool ci18n_set_fallback_in(ci18n_t *catalog, const char *language_code);
#if !defined(CI18N_NO_LOCALE)
    CI18N_DEF bool ci18n_set_current_best_in(ci18n_t *catalog, const char *locale);
#endif
    CI18N_DEF bool ci18n_set_in(ci18n_t *catalog, const char *language_code, const char *key, const char *value);
    CI18N_DEF bool ci18n_remove_in(ci18n_t *catalog, const char *language_code, const char *key);
    CI18N_DEF bool ci18n_clear_in(ci18n_t *catalog, const char *language_code);
    CI18N_DEF bool ci18n_remove_language_in(ci18n_t *catalog, const char *language_code);
    CI18N_DEF const char *ci18n_get_in(ci18n_t *catalog, const char *key);
    CI18N_DEF const char *ci18n_get_or_key_in(ci18n_t *catalog, const char *key);
    CI18N_DEF bool ci18n_has_in(ci18n_t *catalog, const char *key);
    CI18N_DEF const char *ci18n_get_current_in(ci18n_t *catalog);
    CI18N_DEF size_t ci18n_get_languages_in(ci18n_t *catalog, const char **out, size_t capacity);
    CI18N_DEF size_t ci18n_count_in(ci18n_t *catalog, const char *language_code);
    CI18N_DEF size_t ci18n_foreach_in(ci18n_t *catalog, const char *language_code,
                                      ci18n_entry_fn fn, void *user_data);
    CI18N_DEF const char *ci18n_plural_in(ci18n_t *catalog, const char *key, long count);
    CI18N_DEF const char *ci18n_plural_or_key_in(ci18n_t *catalog, const char *key, long count);
    CI18N_DEF ci18n_direction_t ci18n_current_direction_in(ci18n_t *catalog);
#if !defined(CI18N_NO_FORMAT)
    CI18N_DEF bool ci18n_set_bidi_isolation_in(ci18n_t *catalog, bool enabled);
    CI18N_DEF bool ci18n_set_formatter_in(ci18n_t *catalog, const char *name,
                                          ci18n_formatter_fn fn, void *user_data);
    CI18N_DEF bool ci18n_remove_formatter_in(ci18n_t *catalog, const char *name);
    CI18N_DEF size_t ci18n_format_in(ci18n_t *catalog, char *out, size_t capacity,
                                     const char *key, ...);
    CI18N_DEF size_t ci18n_format_plural_in(ci18n_t *catalog, char *out, size_t capacity,
                                            const char *key, long count, ...);
#endif
#if !defined(CI18N_NO_ORDINALS)
    CI18N_DEF const char *ci18n_ordinal_in(ci18n_t *catalog, const char *key, long count);
    CI18N_DEF const char *ci18n_ordinal_or_key_in(ci18n_t *catalog, const char *key, long count);
#endif
#if !defined(CI18N_NO_FORMAT) && !defined(CI18N_NO_ORDINALS)
    CI18N_DEF size_t ci18n_format_ordinal_in(ci18n_t *catalog, char *out, size_t capacity,
                                             const char *key, long count, ...);
#endif

    /* Diagnostics for a specific catalogue. In the shared threading mode the
     * plain versions are per-thread, which is what a caller wants; these
     * report what the catalogue itself last recorded. */
    CI18N_DEF ci18n_error_t ci18n_last_error_in(ci18n_t *catalog);
    CI18N_DEF const ci18n_load_stats_t *ci18n_last_load_stats_in(ci18n_t *catalog);

    /* As ci18n_get_copy(), on the catalogue given. */
    CI18N_DEF size_t ci18n_get_copy_in(ci18n_t *catalog, const char *key,
                                       char *out, size_t capacity);

    /* ============================================================================
     * Diagnostics
     * ============================================================================ */

    /*
     * Why the last call failed.
     *
     * Every function that can fail sets this before returning, and every one
     * that succeeds clears it to CI18N_OK, so read it immediately after the
     * call you are checking. ci18n_init() and ci18n_free() reset it.
     *
     *   if (!ci18n_load_language("en", path)) {
     *       fprintf(stderr, "%s\n", ci18n_error_string(ci18n_last_error()));
     *   }
     *
     * Returns: the last error code, or CI18N_OK
     */
    CI18N_DEF ci18n_error_t ci18n_last_error(void);

    /*
     * A short English description of an error code, for logs.
     *
     * Returns: a static string, never NULL, valid for the program's lifetime
     */
    CI18N_DEF const char *ci18n_error_string(ci18n_error_t error);

    /*
     * What the last ci18n_load_language() or ci18n_load_from_buffer() did.
     *
     * A loader returns true whenever it could read its source, which says
     * nothing about the contents: a file whose every line is malformed still
     * loads successfully with zero entries. Check here to find out, and note
     * that a load which dropped or truncated anything also leaves
     * ci18n_last_error() at CI18N_ERR_PARSE.
     *
     *   ci18n_load_language("en", path);
     *
     *   const ci18n_load_stats_t *st = ci18n_last_load_stats();
     *   if (st->lines_malformed) {
     *       fprintf(stderr, "%s: %u bad lines, first at line %u\n", path,
     *               (unsigned)st->lines_malformed,
     *               (unsigned)st->first_malformed_line);
     *   }
     *
     * Returns: the stats of the last load, zeroed if none has run
     */
    CI18N_DEF const ci18n_load_stats_t *ci18n_last_load_stats(void);

    /* ============================================================================
     * Threads
     * ============================================================================
     *
     * Three modes. Pick one before including this header; defining both of the
     * macros is an error.
     *
     * 1. Default, nothing defined. One global context, no locking. Safe when
     *    every load finished before the threads were spawned and they only
     *    read afterwards, which covers most programs. Concurrent writing
     *    against concurrent reading is a data race.
     *
     * 2. CI18N_THREAD_LOCAL_CONTEXT. Every thread gets its own context. Read
     *    that literally: it is isolation, not sharing. Each thread calls
     *    ci18n_init() and loads its own translations, and a language loaded on
     *    one thread is invisible to the others. Suits a worker rendering in
     *    one user's locale.
     *
     * 3. CI18N_THREAD_SHARED. One shared context behind a reader-writer lock,
     *    which is the "load once, read from many threads, reload occasionally"
     *    case. Readers do not block each other; a writer excludes everyone.
     *
     * In the shared mode, ci18n_last_error() and ci18n_last_load_stats() are
     * per-thread rather than shared, because a diagnostic belongs to the call
     * that produced it. Otherwise one thread's failure would overwrite
     * another's and neither could trust the answer.
     *
     * One thing a lock cannot fix: a `const char *` from ci18n_get() points
     * into storage another thread may reallocate the moment the lock is
     * released. In the shared mode use ci18n_get_copy(), which copies while
     * the lock is still held. See "Pointer lifetime" above.
     */

#ifdef CI18N_THREAD_LOCAL_CONTEXT
    /* Get the calling thread's context for manual control */
    CI18N_DEF ci18n_context_t *ci18n_get_context(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* CI18N_H */

/* ============================================================================
 * IMPLEMENTATION
 *
 * Guarded on its own, apart from the declarations above: a file that defines
 * CI18N_IMPLEMENTATION may still include the header twice, directly and
 * through a compiled catalogue, and the implementation must appear once.
 * ============================================================================ */

#if defined(CI18N_IMPLEMENTATION) && !defined(CI18N_IMPLEMENTATION_INCLUDED)
#define CI18N_IMPLEMENTATION_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* Needed for GetUserDefaultLocaleName(). The shared threading mode needs
 * windows.h too, and pulls it in earlier, next to the lock it declares. */
#if defined(_WIN32) && !defined(CI18N_NO_PLATFORM_LOCALE)
#include <windows.h>
#endif

/* The lock type and its operations are declared with the public types, since
 * the lock is a field of the catalogue. What belongs here is how the library
 * uses them, and the storage for the default catalogue. */
#ifdef CI18N_THREAD_SHARED

/*
 * Which lock shard the calling thread reads through. Threads are numbered
 * round robin the first time they read, so up to 16 threads never share a
 * shard. 0 means "not numbered yet", hence the +1. The counter has its own
 * lock, taken once per thread.
 */
static CI18N_THREAD_LOCAL unsigned ci18n_tls_shard;
static ci18n_rwlock_t ci18n_shard_counter_lock = CI18N_RWLOCK_INIT;
static unsigned ci18n_shard_counter;

static unsigned ci18n_my_shard(void)
{
    if (ci18n_tls_shard == 0)
    {
        ci18n_rwlock_write(&ci18n_shard_counter_lock);
        ci18n_tls_shard = ci18n_shard_counter++ % CI18N_LOCK_SHARDS + 1;
        ci18n_rwlock_write_unlock(&ci18n_shard_counter_lock);
    }
    return ci18n_tls_shard - 1;
}

/* Always in the same order, so two writers cannot deadlock. */
static void ci18n_lock_all(ci18n_context_t *c)
{
    unsigned i;
    for (i = 0; i < CI18N_LOCK_SHARDS; i++)
    {
        ci18n_rwlock_write(&c->locks[i].lock);
    }
}

static void ci18n_unlock_all(ci18n_context_t *c)
{
    unsigned i = CI18N_LOCK_SHARDS;
    while (i-- > 0)
    {
        ci18n_rwlock_write_unlock(&c->locks[i].lock);
    }
}

#define CI18N_READ_LOCK(c) ci18n_rwlock_read(&(c)->locks[ci18n_my_shard()].lock)
#define CI18N_READ_UNLOCK(c) ci18n_rwlock_read_unlock(&(c)->locks[ci18n_my_shard()].lock)
#define CI18N_WRITE_LOCK(c) ci18n_lock_all(c)
#define CI18N_WRITE_UNLOCK(c) ci18n_unlock_all(c)

/*
 * Diagnostics are per-thread even though the data is shared: an error code
 * describes the call that produced it, so sharing one slot would mean two
 * threads overwriting each other and neither being able to trust the answer.
 */
static CI18N_THREAD_LOCAL ci18n_error_t ci18n_tls_error;
static CI18N_THREAD_LOCAL ci18n_load_stats_t ci18n_tls_stats;

/* The slot is thread-local here, so the catalogue is irrelevant. It is still
 * consumed, through a comma expression that yields an lvalue, so that helpers
 * taking a catalogue they do not otherwise need are not left with an unused
 * parameter in this mode alone. */
#define CI18N_ERROR_SLOT(c) (*((void)(c), &ci18n_tls_error))
#define CI18N_STATS_SLOT(c) (*((void)(c), &ci18n_tls_stats))

/*
 * The lock is initialised statically, which is what makes it usable before
 * ci18n_init() and removes any question of who initialises it first.
 *
 * Leaving it merely zeroed was the 2.6.0 bug: on glibc
 * PTHREAD_RWLOCK_INITIALIZER is all zeros so it worked by accident, while on
 * macOS the initialiser carries a signature, a zeroed lock is invalid, and
 * every call returned EINVAL and did nothing.
 */
static ci18n_context_t ci18n_ctx = {.locks = CI18N_LOCK_SHARDS_INIT};

#else /* not CI18N_THREAD_SHARED */

#define CI18N_READ_LOCK(c) ((void)0)
#define CI18N_READ_UNLOCK(c) ((void)0)
#define CI18N_WRITE_LOCK(c) ((void)0)
#define CI18N_WRITE_UNLOCK(c) ((void)0)

#define CI18N_ERROR_SLOT(c) (c)->last_error
#define CI18N_STATS_SLOT(c) (c)->load_stats

#ifdef CI18N_THREAD_LOCAL_CONTEXT
static CI18N_THREAD_LOCAL ci18n_context_t ci18n_ctx;
#else
static ci18n_context_t ci18n_ctx;
#endif

#endif /* CI18N_THREAD_SHARED */

/*
 * Whitespace test that does not depend on the active locale.
 *
 * isspace() classifies bytes above 127 as space in some locales, which would
 * eat the tail of a UTF-8 sequence while trimming a translated value.
 */
static int ci18n_is_space(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r';
}

/*
 * Copy at most cap-1 bytes and terminate.
 *
 * Unlike strncpy() this does not pad the destination with zeros, which matters
 * because a value field is 4 KB wide and would otherwise be rewritten in full
 * on every single assignment.
 */
static void ci18n_copy(char *dst, size_t cap, const char *src)
{
    size_t len = strlen(src);

    if (len > cap - 1)
    {
        len = cap - 1;
    }

    memcpy(dst, src, len);
    dst[len] = '\0';
}

/* Record a failure and return false, so callers stay one line per check */
static bool ci18n_fail(ci18n_t *ctx, ci18n_error_t error)
{
    CI18N_ERROR_SLOT(ctx) = error;
    return false;
}

/* Record success */
static void ci18n_succeed(ci18n_t *ctx)
{
    CI18N_ERROR_SLOT(ctx) = CI18N_OK;
}

/*
 * A language code has to fit the field whole.
 *
 * Truncating here used to be quietly destructive: writes stored the shortened
 * code while lookups compared the full string, so an over-long code could be
 * written and never selected, and a second code sharing the same prefix
 * created a duplicate, permanently unreachable slot.
 */
static bool ci18n_code_fits(const char *code)
{
    return strlen(code) < CI18N_MAX_CODE_LENGTH;
}

/* ============================================================================
 * Storage: a string arena plus a hash table, per language
 * ============================================================================ */

/*
 * FNV-1a, 32 bit. Chosen for being about ten lines and good enough for short
 * ASCII keys, which is what translation keys are.
 */
static uint32_t ci18n_hash(const char *key, size_t len)
{
    uint32_t h = 2166136261u;
    size_t i;

    for (i = 0; i < len; i++)
    {
        h ^= (uint32_t)(unsigned char)key[i];
        h *= 16777619u;
    }

    return h;
}

/* Make room for `need` more bytes in the arena, doubling as it grows. */
static bool ci18n_arena_reserve(ci18n_t *ctx, ci18n_arena_t *arena, size_t need)
{
    size_t capacity = arena->capacity;
    char *data;

    if (arena->used + need <= capacity)
    {
        return true;
    }

    if (capacity == 0)
    {
        capacity = 256;
    }

    while (capacity < arena->used + need)
    {
        capacity *= 2;
    }

    data = (char *)realloc(arena->data, capacity);
    if (!data)
    {
        return ci18n_fail(ctx, CI18N_ERR_OUT_OF_MEMORY);
    }

    arena->data = data;
    arena->capacity = capacity;
    return true;
}

/*
 * Copy a string into the arena and return its offset.
 *
 * Offsets rather than pointers precisely so that growing the arena costs
 * nothing: a realloc moves every string at once and no stored reference has
 * to be corrected.
 */
static bool ci18n_arena_add(ci18n_t *ctx, ci18n_arena_t *arena, const char *text, size_t len,
                            uint32_t *out_offset)
{
    if (!ci18n_arena_reserve(ctx, arena, len + 1))
    {
        return false;
    }

    *out_offset = (uint32_t)arena->used;
    memcpy(arena->data + arena->used, text, len);
    arena->data[arena->used + len] = '\0';
    arena->used += len + 1;
    return true;
}

static const char *ci18n_arena_at(const ci18n_arena_t *arena, uint32_t offset)
{
    return arena->data + offset;
}

/*
 * Give back the spare room that doubling leaves behind.
 *
 * Called once a load is finished, when the language is unlikely to grow
 * again soon. Without it a loaded language kept up to twice the memory it
 * needed, 2 to 2.5 times the size of its file; with it, about 1.4 times.
 * A later ci18n_set() grows by doubling again, as before.
 *
 * A failed shrink is harmless: realloc() leaves the block as it was, and
 * the language simply keeps its spare room.
 */
static void ci18n_language_shrink(ci18n_language_t *lang)
{
    if (lang->strings.used > 0 && lang->strings.used < lang->strings.capacity)
    {
        char *data = (char *)realloc(lang->strings.data, lang->strings.used);
        if (data)
        {
            lang->strings.data = data;
            lang->strings.capacity = lang->strings.used;
        }
    }

    if (lang->count > 0 && lang->count < lang->capacity)
    {
        ci18n_entry_t *entries =
            (ci18n_entry_t *)realloc(lang->entries, lang->count * sizeof(ci18n_entry_t));
        if (entries)
        {
            lang->entries = entries;
            lang->capacity = lang->count;
        }
    }
}

/*
 * Decode one escape sequence.
 *
 * `text` points at the character after the backslash. Returns the byte to
 * emit and sets *consumed to how much of the input it used. An unknown
 * sequence keeps both characters, so a stray backslash in a translation stays
 * visible instead of quietly deleting the letter after it.
 */
static char ci18n_unescape_one(const char *text, size_t remaining, size_t *consumed)
{
    if (remaining == 0)
    {
        *consumed = 0;
        return '\\';
    }

    *consumed = 1;

    switch (text[0])
    {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '\\':
        return '\\';
    case '=':
        return '=';
    case '#':
        return '#';
    case ';':
        return ';';
    case ' ':
        return ' ';
    default:
        break;
    }

    /* Not an escape we know: emit the backslash and leave the rest alone. */
    *consumed = 0;
    return '\\';
}

/* Length the text will have once its escapes are decoded. */
static size_t ci18n_unescaped_length(const char *text, size_t len)
{
    size_t out = 0;
    size_t i = 0;

    while (i < len)
    {
        if (text[i] == '\\')
        {
            size_t consumed;

            ci18n_unescape_one(text + i + 1, len - i - 1, &consumed);
            i += 1 + consumed;
        }
        else
        {
            i++;
        }

        out++;
    }

    return out;
}

/*
 * Copy into the arena, decoding escapes as it goes.
 *
 * Decoding happens here rather than in a scratch buffer because the arena is
 * where the bytes are going anyway, and a translation line has no business
 * needing a second copy of itself.
 */
static bool ci18n_arena_add_unescaped(ci18n_t *ctx, ci18n_arena_t *arena, const char *text, size_t len,
                                      uint32_t *out_offset)
{
    size_t decoded_len = ci18n_unescaped_length(text, len);
    size_t i = 0;
    size_t out;

    if (!ci18n_arena_reserve(ctx, arena, decoded_len + 1))
    {
        return false;
    }

    *out_offset = (uint32_t)arena->used;
    out = arena->used;

    while (i < len)
    {
        if (text[i] == '\\')
        {
            size_t consumed;

            arena->data[out++] = ci18n_unescape_one(text + i + 1, len - i - 1, &consumed);
            i += 1 + consumed;
        }
        else
        {
            arena->data[out++] = text[i++];
        }
    }

    arena->data[out] = '\0';
    arena->used = out + 1;
    return true;
}

/* Link every entry into its bucket from scratch, after the table resized. */
static void ci18n_rebuild_buckets(ci18n_language_t *lang)
{
    size_t mask = lang->bucket_count - 1;
    size_t i;

    for (i = 0; i < lang->bucket_count; i++)
    {
        lang->buckets[i] = CI18N_NO_INDEX;
    }

    for (i = 0; i < lang->count; i++)
    {
        size_t bucket = lang->entries[i].hash & mask;

        lang->entries[i].next = lang->buckets[bucket];
        lang->buckets[bucket] = (uint32_t)i;
    }
}

/*
 * Grow the bucket array when the table gets crowded.
 *
 * Kept at or below a load factor of 0.75, which keeps chains at a couple of
 * entries. Bucket counts are powers of two so the modulo is a mask.
 */
static bool ci18n_ensure_buckets(ci18n_t *ctx, ci18n_language_t *lang, size_t wanted)
{
    size_t bucket_count = lang->bucket_count;
    uint32_t *buckets;

    if (bucket_count > 0 && wanted * 4 <= bucket_count * 3)
    {
        return true;
    }

    if (bucket_count == 0)
    {
        bucket_count = 16;
    }

    while (wanted * 4 > bucket_count * 3)
    {
        bucket_count *= 2;
    }

    buckets = (uint32_t *)realloc(lang->buckets, sizeof(uint32_t) * bucket_count);
    if (!buckets)
    {
        return ci18n_fail(ctx, CI18N_ERR_OUT_OF_MEMORY);
    }

    lang->buckets = buckets;
    lang->bucket_count = bucket_count;
    ci18n_rebuild_buckets(lang);
    return true;
}

/* Make room for one more entry, doubling as it grows. */
static bool ci18n_ensure_capacity(ci18n_t *ctx, ci18n_language_t *lang)
{
    size_t capacity = lang->capacity;
    ci18n_entry_t *entries;

    if (lang->count < capacity)
    {
        return true;
    }

    if (lang->count >= CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        return ci18n_fail(ctx, CI18N_ERR_TOO_MANY_KEYS);
    }

    capacity = capacity == 0 ? 8 : capacity * 2;
    if (capacity > CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        capacity = CI18N_MAX_KEYS_PER_LANGUAGE;
    }

    entries = (ci18n_entry_t *)realloc(lang->entries, sizeof(ci18n_entry_t) * capacity);
    if (!entries)
    {
        return ci18n_fail(ctx, CI18N_ERR_OUT_OF_MEMORY);
    }

    lang->entries = entries;
    lang->capacity = capacity;
    return true;
}

/*
 * Find an entry by key, by length rather than by NUL, so the parser can look
 * up a slice of its line buffer without copying it out first.
 *
 * Returns the entry index, or -1. The cached hash filters out almost every
 * candidate before strcmp is reached.
 */
static int ci18n_find_entry_n(ci18n_language_t *lang, const char *key, size_t key_len,
                              uint32_t hash)
{
    uint32_t index;

    if (lang->bucket_count == 0)
    {
        return -1;
    }

    index = lang->buckets[hash & (lang->bucket_count - 1)];

    while (index != CI18N_NO_INDEX)
    {
        ci18n_entry_t *entry = &lang->entries[index];

        if (entry->hash == hash)
        {
            const char *stored = ci18n_arena_at(&lang->strings, entry->key);

            if (strncmp(stored, key, key_len) == 0 && stored[key_len] == '\0')
            {
                return (int)index;
            }
        }

        index = entry->next;
    }

    return -1;
}

static int ci18n_find_entry(ci18n_language_t *lang, const char *key)
{
    size_t len = strlen(key);

    return ci18n_find_entry_n(lang, key, len, ci18n_hash(key, len));
}

/*
 * Add or update one entry, taking both strings as slices.
 *
 * On update the new value is written over the old one when it fits, which
 * covers the common case of re-loading a file. When it does not fit, the new
 * value is appended and the old bytes stay in the arena as dead weight until
 * the language is cleared.
 */
static bool ci18n_lang_set_ex(ci18n_t *ctx, ci18n_language_t *lang,
                              const char *key, size_t key_len,
                              const char *value, size_t value_len,
                              bool unescape)
{
    uint32_t hash;
    uint32_t key_offset;
    uint32_t value_offset;
    ci18n_entry_t *entry;
    size_t bucket;
    int existing;
    char decoded_key[CI18N_MAX_KEY_LENGTH];
    bool unescape_key = unescape;

    /* Lookups compare decoded keys, so a key written with escapes has to be
     * decoded before it is hashed. This is the one place a scratch buffer
     * earns its keep, and only when the key actually contains a backslash. */
    if (unescape_key && memchr(key, '\\', key_len) != NULL)
    {
        size_t decoded_len = ci18n_unescaped_length(key, key_len);
        uint32_t offset;

        if (decoded_len > sizeof(decoded_key) - 1)
        {
            decoded_len = sizeof(decoded_key) - 1;
        }

        if (!ci18n_arena_add_unescaped(ctx, &lang->strings, key, key_len, &offset))
        {
            return false;
        }

        memcpy(decoded_key, lang->strings.data + offset, decoded_len);
        decoded_key[decoded_len] = '\0';
        lang->strings.used = offset;

        key = decoded_key;
        key_len = decoded_len;

        /* The key is decoded now; the value still is not. These were one
         * flag, which meant a key containing a backslash silently switched
         * decoding off for its own value. */
        unescape_key = false;
    }

    hash = ci18n_hash(key, key_len);
    existing = ci18n_find_entry_n(lang, key, key_len, hash);

    if (existing >= 0)
    {
        size_t stored_len = unescape ? ci18n_unescaped_length(value, value_len) : value_len;

        entry = &lang->entries[existing];

        if (strlen(ci18n_arena_at(&lang->strings, entry->value)) >= stored_len)
        {
            char *slot = lang->strings.data + entry->value;

            if (unescape)
            {
                uint32_t offset;

                /* Decode into the existing slot by decoding to the arena tail
                 * and moving it back, which keeps one implementation of the
                 * escape rules rather than two. */
                if (!ci18n_arena_add_unescaped(ctx, &lang->strings, value, value_len, &offset))
                {
                    return false;
                }

                slot = lang->strings.data + entry->value;
                memcpy(slot, lang->strings.data + offset, stored_len + 1);
                lang->strings.used = offset;
            }
            else
            {
                memcpy(slot, value, value_len);
                slot[value_len] = '\0';
            }

            return true;
        }

        if (unescape)
        {
            if (!ci18n_arena_add_unescaped(ctx, &lang->strings, value, value_len, &value_offset))
            {
                return false;
            }
        }
        else if (!ci18n_arena_add(ctx, &lang->strings, value, value_len, &value_offset))
        {
            return false;
        }

        /* The arena may have moved, but offsets are stable, so only this one
         * field needs updating. */
        lang->entries[existing].value = value_offset;
        return true;
    }

    if (!ci18n_ensure_capacity(ctx, lang))
    {
        return false;
    }

    if (!ci18n_ensure_buckets(ctx, lang, lang->count + 1))
    {
        return false;
    }

    if (unescape_key)
    {
        if (!ci18n_arena_add_unescaped(ctx, &lang->strings, key, key_len, &key_offset))
        {
            return false;
        }
    }
    else if (!ci18n_arena_add(ctx, &lang->strings, key, key_len, &key_offset))
    {
        return false;
    }

    if (unescape)
    {
        if (!ci18n_arena_add_unescaped(ctx, &lang->strings, value, value_len, &value_offset))
        {
            return false;
        }
    }
    else if (!ci18n_arena_add(ctx, &lang->strings, value, value_len, &value_offset))
    {
        return false;
    }

    bucket = hash & (lang->bucket_count - 1);
    entry = &lang->entries[lang->count];
    entry->key = key_offset;
    entry->value = value_offset;
    entry->hash = hash;
    entry->next = lang->buckets[bucket];
    lang->buckets[bucket] = (uint32_t)lang->count;
    lang->count++;

    return true;
}

/* Programmatic values arrive already unescaped by the C compiler, so this
 * path stores them verbatim. Only the file and buffer parsers decode. */
static bool ci18n_lang_set(ci18n_t *ctx, ci18n_language_t *lang,
                           const char *key, size_t key_len,
                           const char *value, size_t value_len)
{
    return ci18n_lang_set_ex(ctx, lang, key, key_len, value, value_len, false);
}

/* Release everything one language holds. */
static void ci18n_lang_release(ci18n_language_t *lang)
{
    free(lang->strings.data);
    free(lang->entries);
    free(lang->buckets);
    memset(lang, 0, sizeof(*lang));
}

/* Find language by code, returns index or -1 if not found */
static int ci18n_find_language(ci18n_t *ctx, const char *code)
{
    size_t i;
    for (i = 0; i < ctx->language_count; i++)
    {
        if (strcmp(ctx->languages[i].code, code) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

/*
 * Get or create a language.
 *
 * Nothing is allocated here: an untouched language costs only its slot in the
 * context, and the arena, entries and buckets appear on first insert. That
 * matters on a device where the old fixed layout claimed 272 KB before
 * storing a single translation.
 */
static ci18n_language_t *ci18n_get_or_create_language(ci18n_t *ctx, const char *code)
{
    int idx = ci18n_find_language(ctx, code);
    ci18n_language_t *lang;

    if (idx >= 0)
    {
        return &ctx->languages[idx];
    }

    if (ctx->language_count >= CI18N_MAX_LANGUAGES)
    {
        ci18n_fail(ctx, CI18N_ERR_TOO_MANY_LANGUAGES);
        return NULL;
    }

    lang = &ctx->languages[ctx->language_count];
    memset(lang, 0, sizeof(ci18n_language_t));
    ci18n_copy(lang->code, sizeof(lang->code), code);
    ctx->language_count++;

    return lang;
}

/* What one line turned into, so a loader can keep count */
typedef enum ci18n_line_result
{
    CI18N_LINE_LOADED,    /* an entry was added or updated */
    CI18N_LINE_SKIPPED,   /* blank line or comment */
    CI18N_LINE_MALFORMED, /* no separator, or an empty key */
    CI18N_LINE_FAILED     /* out of room or out of memory */
} ci18n_line_result_t;

/* Trim a slice in place by moving its ends, rather than copying it out. */
static void ci18n_trim_slice(const char **text, size_t *len)
{
    const char *start = *text;
    size_t n = *len;

    while (n > 0 && ci18n_is_space(*start))
    {
        start++;
        n--;
    }

    while (n > 0 && ci18n_is_space(start[n - 1]))
    {
        /* An escaped space is content, not padding, which is the only way to
         * keep a trailing space that trimming would otherwise eat. */
        if (n >= 2 && start[n - 2] == '\\')
        {
            break;
        }

        n--;
    }

    *text = start;
    *len = n;
}

/*
 * Parse one line straight out of the caller's buffer.
 *
 * Nothing is copied here: the key and the value are handed to the arena as
 * slices. The previous version kept a 256 byte and a 4096 byte buffer on the
 * stack for every line, which is a lot to ask of a small device for a parser
 * that never needed either.
 */
static ci18n_line_result_t ci18n_parse_line(ci18n_t *ctx, ci18n_language_t *lang, const char *line)
{
    const char *eq;
    const char *key;
    const char *value;
    size_t key_len;
    size_t value_len;

    /* Skip a UTF-8 BOM. Editors on Windows often prepend one, and without this
     * the first key of the file would silently become "\xEF\xBB\xBFkey" and be
     * unreachable through ci18n_get(). */
    if ((unsigned char)line[0] == 0xEF &&
        (unsigned char)line[1] == 0xBB &&
        (unsigned char)line[2] == 0xBF)
    {
        line += 3;
    }

    /* Skip empty lines and comments. A key that genuinely starts with # or ;
     * is written \# or \; and reaches the parser below. */
    while (*line && ci18n_is_space(*line))
        line++;
    if (*line == '\0' || *line == '#' || *line == ';')
    {
        return CI18N_LINE_SKIPPED;
    }

    /* Find the separator, skipping any that is escaped, so a key may contain
     * an equals sign by writing \=. */
    eq = NULL;
    {
        size_t scan = 0;

        while (line[scan] != '\0')
        {
            if (line[scan] == '\\' && line[scan + 1] != '\0')
            {
                scan += 2;
                continue;
            }

            if (line[scan] == '=')
            {
                eq = line + scan;
                break;
            }

            scan++;
        }
    }

    if (!eq)
    {
        return CI18N_LINE_MALFORMED;
    }

    key = line;
    key_len = (size_t)(eq - line);
    ci18n_trim_slice(&key, &key_len);

    if (key_len == 0)
    {
        return CI18N_LINE_MALFORMED;
    }

    value = eq + 1;
    value_len = strlen(value);
    ci18n_trim_slice(&value, &value_len);

    /* Clamp to the configured limits, counting what had to be cut. Trimming
     * happens first, so surrounding whitespace never costs a key its tail. */
    if (key_len > CI18N_MAX_KEY_LENGTH - 1)
    {
        key_len = CI18N_MAX_KEY_LENGTH - 1;
        CI18N_STATS_SLOT(ctx).keys_truncated++;
    }

    if (value_len > CI18N_MAX_VALUE_LENGTH - 1)
    {
        value_len = CI18N_MAX_VALUE_LENGTH - 1;
        CI18N_STATS_SLOT(ctx).values_truncated++;
    }

    if (!ci18n_lang_set_ex(ctx, lang, key, key_len, value, value_len, true))
    {
        return CI18N_LINE_FAILED;
    }

    return CI18N_LINE_LOADED;
}

/* Fold one line's outcome into the stats of the load in progress */
static void ci18n_record_line(ci18n_t *ctx, ci18n_line_result_t result, size_t line_number)
{
    CI18N_STATS_SLOT(ctx).lines_read++;

    switch (result)
    {
    case CI18N_LINE_LOADED:
        CI18N_STATS_SLOT(ctx).entries_loaded++;
        break;

    case CI18N_LINE_SKIPPED:
        CI18N_STATS_SLOT(ctx).lines_skipped++;
        break;

    case CI18N_LINE_MALFORMED:
    case CI18N_LINE_FAILED:
        CI18N_STATS_SLOT(ctx).lines_malformed++;
        if (CI18N_STATS_SLOT(ctx).first_malformed_line == 0)
        {
            CI18N_STATS_SLOT(ctx).first_malformed_line = line_number;
        }
        break;
    }
}

/* Start a load with a clean slate of statistics */
static void ci18n_reset_load_stats(ci18n_t *ctx)
{
    memset(&CI18N_STATS_SLOT(ctx), 0, sizeof(CI18N_STATS_SLOT(ctx)));
}

/*
 * Close out a load. Reading the source counts as success, so a file of pure
 * garbage still returns true; anything dropped or truncated is reported
 * through the error code and the stats instead.
 */
static bool ci18n_finish_load(ci18n_t *ctx)
{
    const ci18n_load_stats_t *st = &CI18N_STATS_SLOT(ctx);

    if (st->lines_malformed > 0 || st->keys_truncated > 0 ||
        st->values_truncated > 0 || st->lines_truncated > 0)
    {
        CI18N_ERROR_SLOT(ctx) = CI18N_ERR_PARSE;
    }
    else
    {
        ci18n_succeed(ctx);
    }

    return true;
}

/*
 * Clear a catalogue's data, field by field.
 *
 * Not a memset over the whole struct, for two reasons. The lock is a field
 * now, and zeroing it while it is held is undefined; and a memset of the
 * global would be wrong the moment there is more than one catalogue.
 */
static void ci18n_reset_data(ci18n_t *ctx)
{
    memset(ctx->languages, 0, sizeof(ctx->languages));
    ctx->language_count = 0;
    ctx->current_language[0] = 0;
    ctx->fallback_language[0] = 0;
    ctx->last_error = CI18N_OK;
    memset(&ctx->load_stats, 0, sizeof(ctx->load_stats));
#ifndef CI18N_NO_FORMAT
    memset(ctx->formatters, 0, sizeof(ctx->formatters));
    ctx->formatter_count = 0;
    ctx->bidi_isolation = false;
#endif
    ctx->initialized = false;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

static bool ci18n_init_impl(ci18n_t *ctx)
{
    if (ctx->initialized)
    {
        return true;
    }

    ci18n_reset_data(ctx);
    ctx->initialized = true;
    return true;
}
CI18N_DEF bool ci18n_init(void)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_init_impl(&ci18n_ctx);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}


static void ci18n_free_impl(ci18n_t *ctx)
{
    size_t i;

    if (!ctx->initialized)
    {
        return;
    }

    for (i = 0; i < ctx->language_count; i++)
    {
        ci18n_lang_release(&ctx->languages[i]);
    }

    ci18n_reset_data(ctx);
}
CI18N_DEF void ci18n_free(void)
{
    CI18N_WRITE_LOCK(&ci18n_ctx);
    ci18n_free_impl(&ci18n_ctx);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);
}


/*
 * Splits bytes into lines for the parser, whatever size the pieces arrive
 * in. Both loaders feed it, so a file and the same bytes in a buffer give
 * the same entries: LF, CRLF and a lone CR all end a line, and a line past
 * CI18N_MAX_LINE_LENGTH keeps its head and loses its tail.
 *
 * The file loader used fgets() once, which differed from the buffer loader
 * on lone CRs, and which some embedded C libraries get wrong: picolibc, the
 * default in Zephyr, returns NULL for a last line with no newline and drops
 * what it read.
 */
typedef struct ci18n_lines
{
    char line[CI18N_MAX_LINE_LENGTH];
    size_t pos;
    size_t number;
    bool cut;         /* this line overflowed, the rest of it is dropped */
    bool after_cr;    /* the previous byte ended a line with CR */
} ci18n_lines_t;

static void ci18n_lines_emit(ci18n_t *ctx, ci18n_language_t *lang, ci18n_lines_t *st)
{
    st->line[st->pos] = '\0';
    st->number++;
    ci18n_record_line(ctx, ci18n_parse_line(ctx, lang, st->line), st->number);
    st->pos = 0;
    st->cut = false;
}

static void ci18n_lines_feed(ci18n_t *ctx, ci18n_language_t *lang, ci18n_lines_t *st,
                             const char *data, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++)
    {
        char c = data[i];

        /* The LF of a CRLF pair, possibly in the next piece. */
        if (st->after_cr)
        {
            st->after_cr = false;
            if (c == '\n')
            {
                continue;
            }
        }

        if (c == '\n' || c == '\r')
        {
            ci18n_lines_emit(ctx, lang, st);
            st->after_cr = (c == '\r');
        }
        else if (st->pos < CI18N_MAX_LINE_LENGTH - 1)
        {
            st->line[st->pos++] = c;
        }
        else if (!st->cut)
        {
            CI18N_STATS_SLOT(ctx).lines_truncated++;
            st->cut = true;
        }
    }
}

/* The last line, when the input does not end with a newline. */
static void ci18n_lines_finish(ci18n_t *ctx, ci18n_language_t *lang, ci18n_lines_t *st)
{
    if (st->pos > 0 || st->cut)
    {
        ci18n_lines_emit(ctx, lang, st);
    }
}

/*
 * The Windows CRT marks fopen and getenv deprecated in favour of its _s
 * variants, and a warning from this header would stop any project built
 * with warnings as errors. They are silenced around each call rather than by
 * defining _CRT_SECURE_NO_WARNINGS, which would cover the caller's own code
 * too. MSVC and clang-cl both define _MSC_VER, but clang-cl ignores MSVC's
 * warning pragmas and needs its own.
 */
#if defined(_MSC_VER) && defined(__clang__)
#define CI18N_CRT_WARNINGS_OFF                                              \
    _Pragma("clang diagnostic push")                                        \
        _Pragma("clang diagnostic ignored \"-Wdeprecated-declarations\"")
#define CI18N_CRT_WARNINGS_ON _Pragma("clang diagnostic pop")
#elif defined(_MSC_VER)
#define CI18N_CRT_WARNINGS_OFF __pragma(warning(push)) __pragma(warning(disable : 4996))
#define CI18N_CRT_WARNINGS_ON __pragma(warning(pop))
#else
#define CI18N_CRT_WARNINGS_OFF
#define CI18N_CRT_WARNINGS_ON
#endif

#if !defined(CI18N_NO_FILES)
static bool ci18n_load_language_impl(ci18n_t *ctx, const char *language_code, const char *filepath)
{
    FILE *file;
    ci18n_lines_t *lines;
    ci18n_language_t *lang;
    char chunk[512];
    size_t got;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !filepath)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    if (!ci18n_code_fits(language_code))
    {
        return ci18n_fail(ctx, CI18N_ERR_CODE_TOO_LONG);
    }

    /* The Windows CRT deprecates fopen in favour of fopen_s; see
     * CI18N_CRT_WARNINGS_OFF. The return value is checked, and nothing
     * unbounded is written. */
    CI18N_CRT_WARNINGS_OFF
    file = fopen(filepath, "rb");
    CI18N_CRT_WARNINGS_ON

    if (!file)
    {
        return ci18n_fail(ctx, CI18N_ERR_FILE_NOT_FOUND);
    }

    /* Created only after the file opened, so a missing file leaves no empty
     * language behind. */
    lang = ci18n_get_or_create_language(ctx, language_code);
    if (!lang)
    {
        fclose(file);
        return false; /* get_or_create already recorded why */
    }

    /* The line buffer is a few KB, too much for a small device's stack to
     * take on top of the chunk, so it comes from the heap. */
    lines = (ci18n_lines_t *)calloc(1, sizeof(*lines));
    if (!lines)
    {
        fclose(file);
        return ci18n_fail(ctx, CI18N_ERR_OUT_OF_MEMORY);
    }

    ci18n_reset_load_stats(ctx);

    while ((got = fread(chunk, 1, sizeof(chunk), file)) > 0)
    {
        ci18n_lines_feed(ctx, lang, lines, chunk, got);
    }
    ci18n_lines_finish(ctx, lang, lines);

    free(lines);
    fclose(file);
    ci18n_language_shrink(lang);
    return ci18n_finish_load(ctx);
}
CI18N_DEF bool ci18n_load_language(const char *language_code, const char *filepath)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_load_language_impl(&ci18n_ctx, language_code, filepath);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}
#endif


static bool ci18n_load_from_buffer_impl(ci18n_t *ctx, const char *language_code, const char *buffer, size_t length)
{
    ci18n_language_t *lang;
    ci18n_lines_t *lines;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !buffer)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    if (!ci18n_code_fits(language_code))
    {
        return ci18n_fail(ctx, CI18N_ERR_CODE_TOO_LONG);
    }

    lang = ci18n_get_or_create_language(ctx, language_code);
    if (!lang)
    {
        return false; /* get_or_create already recorded why */
    }

    lines = (ci18n_lines_t *)calloc(1, sizeof(*lines));
    if (!lines)
    {
        return ci18n_fail(ctx, CI18N_ERR_OUT_OF_MEMORY);
    }

    ci18n_reset_load_stats(ctx);
    ci18n_lines_feed(ctx, lang, lines, buffer, length);
    ci18n_lines_finish(ctx, lang, lines);
    free(lines);

    ci18n_language_shrink(lang);
    return ci18n_finish_load(ctx);
}
CI18N_DEF bool ci18n_load_from_buffer(const char *language_code, const char *buffer, size_t length)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_load_from_buffer_impl(&ci18n_ctx, language_code, buffer, length);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}

#ifndef CI18N_NO_MO
/* ============================================================================
 * gettext .mo catalogues
 * ============================================================================
 *
 * The layout, all fields 32-bit in the file's byte order:
 *
 *   0   magic 0x950412de       16  offset of the translation table
 *   4   revision, major 0      20  hash table size, unused here
 *   8   number of strings      24  hash table offset, unused here
 *   12  offset of the original table
 *
 * Each table holds (length, offset) pairs. A length excludes the NUL that
 * follows the string. A plural original is "singular\0plural", its
 * translation the forms joined by NULs, and a context is "ctx\4msgid".
 */

typedef struct ci18n_mo
{
    const unsigned char *data;
    size_t size;
    bool big_endian;
} ci18n_mo_t;

static uint32_t ci18n_mo_u32(const ci18n_mo_t *mo, size_t at)
{
    const unsigned char *p = mo->data + at;

    if (mo->big_endian)
    {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
               ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }
    return ((uint32_t)p[3] << 24) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[1] << 8) | (uint32_t)p[0];
}

/* String `index` of the table at `table`, or false if it points outside the
 * file. The caller has already checked that the table itself fits. */
static bool ci18n_mo_string(const ci18n_mo_t *mo, uint32_t table, uint32_t index,
                            const char **text, size_t *len)
{
    size_t length = ci18n_mo_u32(mo, (size_t)table + (size_t)index * 8);
    size_t offset = ci18n_mo_u32(mo, (size_t)table + (size_t)index * 8 + 4);

    if (offset > mo->size || length > mo->size - offset)
    {
        return false;
    }

    *text = (const char *)mo->data + offset;
    *len = length;
    return true;
}

static bool ci18n_mo_table_fits(const ci18n_mo_t *mo, uint32_t table, uint32_t count)
{
    return table <= mo->size && count <= (mo->size - table) / 8;
}

/* nplurals from the header entry, or 0 when it does not say. The header is
 * not NUL-terminated as far as the bounds go, so the search is by length. */
static unsigned ci18n_mo_nplurals(const char *header, size_t len)
{
    static const char needle[] = "nplurals=";
    const size_t needle_len = sizeof(needle) - 1;
    size_t i;

    for (i = 0; i + needle_len < len; i++)
    {
        if (memcmp(header + i, needle, needle_len) == 0)
        {
            unsigned n = 0;

            i += needle_len;
            while (i < len && header[i] == ' ')
            {
                i++;
            }
            while (i < len && header[i] >= '0' && header[i] <= '9' && n < 100)
            {
                n = n * 10 + (unsigned)(header[i] - '0');
                i++;
            }
            return n;
        }
    }
    return 0;
}

/* gettext numbers plural forms, ci18n names them. The order CLDR categories
 * take for each nplurals, matching tools/po2ci18n.py. */
static const char *ci18n_mo_category(unsigned nplurals, size_t index)
{
    static const char *const orders[6][6] = {
        {"other"},
        {"one", "other"},
        {"one", "few", "many"},
        {"one", "few", "many", "other"},
        {"zero", "one", "two", "few", "many"},
        {"zero", "one", "two", "few", "many", "other"},
    };

    if (nplurals < 1 || nplurals > 6 || index >= nplurals)
    {
        return NULL;
    }
    return orders[nplurals - 1][index];
}

/* One entry into the language. The key is msgid, or "ctx.msgid". */
static ci18n_line_result_t ci18n_mo_entry(ci18n_t *ctx, ci18n_language_t *lang,
                                          unsigned nplurals,
                                          const char *orig, size_t orig_len,
                                          const char *trans, size_t trans_len)
{
    /* Room for "[many]" after a key cut to the limit. */
    char key[CI18N_MAX_KEY_LENGTH + 8];
    const char *msgid = orig;
    const char *nul = (const char *)memchr(orig, '\0', orig_len);
    const char *eot;
    size_t msgid_len = nul ? (size_t)(nul - orig) : orig_len;
    size_t key_max = CI18N_MAX_KEY_LENGTH - 1;
    size_t key_len = 0;
    size_t pos;
    size_t index;
    bool any = false;

    if (msgid_len == 0 || trans_len == 0)
    {
        return CI18N_LINE_SKIPPED;
    }

    if (nul)
    {
        /* Plural: keep room for the longest suffix inside the key limit. */
        key_max = (CI18N_MAX_KEY_LENGTH - 1 > 7) ? CI18N_MAX_KEY_LENGTH - 1 - 7 : 0;
    }

    /* "ctx\4msgid" becomes "ctx.msgid". */
    eot = (const char *)memchr(msgid, '\4', msgid_len);
    if (eot)
    {
        size_t ctx_len = (size_t)(eot - msgid);

        memcpy(key, msgid, ctx_len < key_max ? ctx_len : key_max);
        key_len = ctx_len < key_max ? ctx_len : key_max;
        if (key_len < key_max)
        {
            key[key_len++] = '.';
        }
        msgid_len -= ctx_len + 1;
        msgid = eot + 1;
    }

    if (msgid_len > key_max - key_len)
    {
        msgid_len = key_max - key_len;
        CI18N_STATS_SLOT(ctx).keys_truncated++;
    }
    memcpy(key + key_len, msgid, msgid_len);
    key_len += msgid_len;

    if (!nul)
    {
        if (trans_len > CI18N_MAX_VALUE_LENGTH - 1)
        {
            trans_len = CI18N_MAX_VALUE_LENGTH - 1;
            CI18N_STATS_SLOT(ctx).values_truncated++;
        }
        return ci18n_lang_set(ctx, lang, key, key_len, trans, trans_len)
                   ? CI18N_LINE_LOADED
                   : CI18N_LINE_FAILED;
    }

    /* Plural forms, one per NUL-separated piece of the translation. */
    for (pos = 0, index = 0; pos <= trans_len; index++)
    {
        const char *form = trans + pos;
        const char *end = (const char *)memchr(form, '\0', trans_len - pos);
        size_t form_len = end ? (size_t)(end - form) : trans_len - pos;
        const char *category = ci18n_mo_category(nplurals, index);

        pos += form_len + 1;

        if (!category || form_len == 0)
        {
            continue;
        }

        {
            size_t cat_len = strlen(category);
            size_t full_len = key_len + cat_len + 2;

            key[key_len] = '[';
            memcpy(key + key_len + 1, category, cat_len);
            key[full_len - 1] = ']';

            if (form_len > CI18N_MAX_VALUE_LENGTH - 1)
            {
                form_len = CI18N_MAX_VALUE_LENGTH - 1;
                CI18N_STATS_SLOT(ctx).values_truncated++;
            }
            if (!ci18n_lang_set(ctx, lang, key, full_len, form, form_len))
            {
                return CI18N_LINE_FAILED;
            }
            any = true;
        }
    }

    return any ? CI18N_LINE_LOADED : CI18N_LINE_SKIPPED;
}

static bool ci18n_load_mo_from_buffer_impl(ci18n_t *ctx, const char *language_code,
                                           const void *data, size_t length)
{
    ci18n_language_t *lang;
    ci18n_mo_t mo;
    uint32_t magic;
    uint32_t count;
    uint32_t originals;
    uint32_t translations;
    unsigned nplurals = 0;
    uint32_t i;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !data)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    if (!ci18n_code_fits(language_code))
    {
        return ci18n_fail(ctx, CI18N_ERR_CODE_TOO_LONG);
    }

    mo.data = (const unsigned char *)data;
    mo.size = length;
    mo.big_endian = false;

    /* The whole structure is checked before anything is stored, so a file
     * that is not a catalogue leaves the language untouched. */
    if (length < 28)
    {
        return ci18n_fail(ctx, CI18N_ERR_PARSE);
    }

    magic = ci18n_mo_u32(&mo, 0);
    if (magic != 0x950412deU)
    {
        mo.big_endian = true;
        if (ci18n_mo_u32(&mo, 0) != 0x950412deU)
        {
            return ci18n_fail(ctx, CI18N_ERR_PARSE);
        }
    }

    /* Only the major revision changes the layout. */
    if ((ci18n_mo_u32(&mo, 4) >> 16) != 0)
    {
        return ci18n_fail(ctx, CI18N_ERR_PARSE);
    }

    count = ci18n_mo_u32(&mo, 8);
    originals = ci18n_mo_u32(&mo, 12);
    translations = ci18n_mo_u32(&mo, 16);

    if (!ci18n_mo_table_fits(&mo, originals, count) ||
        !ci18n_mo_table_fits(&mo, translations, count))
    {
        return ci18n_fail(ctx, CI18N_ERR_PARSE);
    }

    lang = ci18n_get_or_create_language(ctx, language_code);
    if (!lang)
    {
        return false; /* get_or_create already recorded why */
    }

    ci18n_reset_load_stats(ctx);

    /* The header is the entry with an empty msgid. msgfmt sorts it first,
     * but a search costs nothing and trusts nothing. */
    for (i = 0; i < count; i++)
    {
        const char *orig;
        const char *trans;
        size_t orig_len;
        size_t trans_len;

        if (ci18n_mo_string(&mo, originals, i, &orig, &orig_len) && orig_len == 0 &&
            ci18n_mo_string(&mo, translations, i, &trans, &trans_len))
        {
            nplurals = ci18n_mo_nplurals(trans, trans_len);
            break;
        }
    }

    for (i = 0; i < count; i++)
    {
        const char *orig;
        const char *trans;
        size_t orig_len;
        size_t trans_len;
        ci18n_line_result_t result;

        if (!ci18n_mo_string(&mo, originals, i, &orig, &orig_len) ||
            !ci18n_mo_string(&mo, translations, i, &trans, &trans_len))
        {
            result = CI18N_LINE_MALFORMED;
        }
        else
        {
            result = ci18n_mo_entry(ctx, lang, nplurals, orig, orig_len, trans, trans_len);
        }

        ci18n_record_line(ctx, result, (size_t)i + 1);
    }

    ci18n_language_shrink(lang);
    return ci18n_finish_load(ctx);
}

#if !defined(CI18N_NO_FILES)
static bool ci18n_load_mo_impl(ci18n_t *ctx, const char *language_code, const char *filepath)
{
    FILE *file;
    unsigned char *data;
    long size;
    size_t got;
    bool result;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !filepath)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    /* See ci18n_load_language_impl for why the warning is silenced here. */
    CI18N_CRT_WARNINGS_OFF
    file = fopen(filepath, "rb");
    CI18N_CRT_WARNINGS_ON

    if (!file)
    {
        return ci18n_fail(ctx, CI18N_ERR_FILE_NOT_FOUND);
    }

    /* Read whole: the format is offsets into the file, so it cannot be
     * streamed the way the line format is. */
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0 ||
        fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return ci18n_fail(ctx, CI18N_ERR_FILE_NOT_FOUND);
    }

    data = (unsigned char *)malloc(size > 0 ? (size_t)size : 1);
    if (!data)
    {
        fclose(file);
        return ci18n_fail(ctx, CI18N_ERR_OUT_OF_MEMORY);
    }

    got = fread(data, 1, (size_t)size, file);
    fclose(file);

    result = ci18n_load_mo_from_buffer_impl(ctx, language_code, data, got);
    free(data);
    return result;
}

CI18N_DEF bool ci18n_load_mo_in(ci18n_t *catalog, const char *language_code, const char *filepath)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_load_mo_impl(catalog, language_code, filepath);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_load_mo(const char *language_code, const char *filepath)
{
    return ci18n_load_mo_in(&ci18n_ctx, language_code, filepath);
}
#endif

CI18N_DEF bool ci18n_load_mo_from_buffer_in(ci18n_t *catalog, const char *language_code,
                                            const void *data, size_t length)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_load_mo_from_buffer_impl(catalog, language_code, data, length);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_load_mo_from_buffer(const char *language_code, const void *data, size_t length)
{
    return ci18n_load_mo_from_buffer_in(&ci18n_ctx, language_code, data, length);
}
#endif /* CI18N_NO_MO */


static bool ci18n_set_current_impl(ci18n_t *ctx, const char *language_code)
{
    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    if (ci18n_find_language(ctx, language_code) < 0)
    {
        return ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    ci18n_copy(ctx->current_language, sizeof(ctx->current_language), language_code);
    ci18n_succeed(ctx);
    return true;
}
CI18N_DEF bool ci18n_set_current(const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_set_current_impl(&ci18n_ctx, language_code);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}


static bool ci18n_set_fallback_impl(ci18n_t *ctx, const char *language_code)
{
    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    if (ci18n_find_language(ctx, language_code) < 0)
    {
        return ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    ci18n_copy(ctx->fallback_language, sizeof(ctx->fallback_language), language_code);
    ci18n_succeed(ctx);
    return true;
}
CI18N_DEF bool ci18n_set_fallback(const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_set_fallback_impl(&ci18n_ctx, language_code);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}


static const char *ci18n_get_impl(ci18n_t *ctx, const char *key)
{
    int lang_idx;
    ci18n_language_t *lang;
    int entry_idx;

    if (!ctx->initialized)
    {
        ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
        return NULL;
    }

    if (!key)
    {
        ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
        return NULL;
    }

    /* Try current language */
    if (ctx->current_language[0] != '\0')
    {
        lang_idx = ci18n_find_language(ctx, ctx->current_language);
        if (lang_idx >= 0)
        {
            lang = &ctx->languages[lang_idx];
            entry_idx = ci18n_find_entry(lang, key);
            if (entry_idx >= 0)
            {
                ci18n_succeed(ctx);
                return ci18n_arena_at(&lang->strings, lang->entries[entry_idx].value);
            }
        }
    }

    /* Try fallback language */
    if (ctx->fallback_language[0] != '\0')
    {
        lang_idx = ci18n_find_language(ctx, ctx->fallback_language);
        if (lang_idx >= 0)
        {
            lang = &ctx->languages[lang_idx];
            entry_idx = ci18n_find_entry(lang, key);
            if (entry_idx >= 0)
            {
                ci18n_succeed(ctx);
                return ci18n_arena_at(&lang->strings, lang->entries[entry_idx].value);
            }
        }
    }

    ci18n_fail(ctx, CI18N_ERR_KEY_NOT_FOUND);
    return NULL;
}
CI18N_DEF const char *ci18n_get(const char *key)
{
    const char *result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_get_impl(&ci18n_ctx, key);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


static const char *ci18n_get_or_key_impl(ci18n_t *ctx, const char *key)
{
    const char *result = ci18n_get_impl(ctx, key);
    return result ? result : key;
}
CI18N_DEF const char *ci18n_get_or_key(const char *key)
{
    const char *result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_get_or_key_impl(&ci18n_ctx, key);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


static bool ci18n_has_impl(ci18n_t *ctx, const char *key)
{
    ci18n_error_t before = CI18N_ERROR_SLOT(ctx);
    bool found = ci18n_get_impl(ctx, key) != NULL;

    /* Asking is not failing: a miss here must not look like a failed call. */
    if (!found && CI18N_ERROR_SLOT(ctx) == CI18N_ERR_KEY_NOT_FOUND)
    {
        CI18N_ERROR_SLOT(ctx) = before;
    }

    return found;
}
CI18N_DEF size_t ci18n_get_copy(const char *key, char *out, size_t capacity)
{
    const char *text;
    size_t len;

    if (out && capacity > 0)
    {
        out[0] = 0;
    }

    /* The copy happens inside the lock. That is the whole point: a pointer
     * handed back to the caller could be invalidated by a writer the instant
     * the lock is released, and a copy cannot. */
    CI18N_READ_LOCK(&ci18n_ctx);

    text = ci18n_get_impl(&ci18n_ctx, key);
    if (!text)
    {
        CI18N_READ_UNLOCK(&ci18n_ctx);
        return 0;
    }

    len = strlen(text);

    if (out && capacity > 0)
    {
        size_t fits = len < capacity - 1 ? len : capacity - 1;

        memcpy(out, text, fits);
        out[fits] = 0;
    }

    CI18N_READ_UNLOCK(&ci18n_ctx);
    return len;
}

CI18N_DEF bool ci18n_has(const char *key)
{
    bool result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_has_impl(&ci18n_ctx, key);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


static const char *ci18n_get_current_impl(ci18n_t *ctx)
{
    if (!ctx->initialized)
    {
        return "";
    }
    return ctx->current_language;
}
CI18N_DEF const char *ci18n_get_current(void)
{
    const char *result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_get_current_impl(&ci18n_ctx);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


static size_t ci18n_get_languages_impl(ci18n_t *ctx, const char **out, size_t capacity)
{
    size_t i;
    size_t n;

    if (!ctx->initialized)
    {
        ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
        return 0;
    }

    ci18n_succeed(ctx);

    if (out)
    {
        n = capacity < ctx->language_count ? capacity : ctx->language_count;
        for (i = 0; i < n; i++)
        {
            out[i] = ctx->languages[i].code;
        }
    }

    return ctx->language_count;
}
CI18N_DEF size_t ci18n_get_languages(const char **out, size_t capacity)
{
    size_t result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_get_languages_impl(&ci18n_ctx, out, capacity);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


static bool ci18n_set_impl(ci18n_t *ctx, const char *language_code, const char *key, const char *value)
{
    ci18n_language_t *lang;
    size_t key_len;
    size_t value_len;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !key || !value)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    if (!ci18n_code_fits(language_code))
    {
        return ci18n_fail(ctx, CI18N_ERR_CODE_TOO_LONG);
    }

    lang = ci18n_get_or_create_language(ctx, language_code);
    if (!lang)
    {
        return false; /* get_or_create already recorded why */
    }

    /* Same clamping the parser applies, so both routes store the same thing. */
    key_len = strlen(key);
    if (key_len > CI18N_MAX_KEY_LENGTH - 1)
    {
        key_len = CI18N_MAX_KEY_LENGTH - 1;
    }

    value_len = strlen(value);
    if (value_len > CI18N_MAX_VALUE_LENGTH - 1)
    {
        value_len = CI18N_MAX_VALUE_LENGTH - 1;
    }

    if (!ci18n_lang_set(ctx, lang, key, key_len, value, value_len))
    {
        return false; /* the storage layer already recorded why */
    }

    ci18n_succeed(ctx);
    return true;
}
CI18N_DEF bool ci18n_set(const char *language_code, const char *key, const char *value)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_set_impl(&ci18n_ctx, language_code, key, value);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}


static bool ci18n_remove_impl(ci18n_t *ctx, const char *language_code, const char *key)
{
    int lang_idx;
    ci18n_language_t *lang;
    int entry_idx;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !key)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    lang_idx = ci18n_find_language(ctx, language_code);
    if (lang_idx < 0)
    {
        return ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    lang = &ctx->languages[lang_idx];
    entry_idx = ci18n_find_entry(lang, key);

    if (entry_idx < 0)
    {
        return ci18n_fail(ctx, CI18N_ERR_KEY_NOT_FOUND);
    }

    /* Move the last entry into the hole rather than shifting everything down.
     * Buckets hold indices, so a shift would invalidate every index above the
     * hole; this only disturbs one. Insertion order is not preserved, and
     * nothing observable depends on it.
     *
     * The strings stay in the arena as dead weight until ci18n_clear() or
     * ci18n_free(), which is the price of packing them contiguously. */
    lang->count--;
    if ((size_t)entry_idx != lang->count)
    {
        lang->entries[entry_idx] = lang->entries[lang->count];
    }

    ci18n_rebuild_buckets(lang);

    ci18n_succeed(ctx);
    return true;
}
CI18N_DEF bool ci18n_remove(const char *language_code, const char *key)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_remove_impl(&ci18n_ctx, language_code, key);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}


static bool ci18n_clear_impl(ci18n_t *ctx, const char *language_code)
{
    int lang_idx;
    ci18n_language_t *lang;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    lang_idx = ci18n_find_language(ctx, language_code);
    if (lang_idx < 0)
    {
        return ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    lang = &ctx->languages[lang_idx];

    /* Clear current/fallback if being cleared */
    if (strcmp(ctx->current_language, language_code) == 0)
    {
        ctx->current_language[0] = '\0';
    }
    if (strcmp(ctx->fallback_language, language_code) == 0)
    {
        ctx->fallback_language[0] = '\0';
    }

    /* Unlike removing entries one at a time, clearing reclaims the strings:
     * the arena is rewound rather than freed, so the next load reuses the
     * memory instead of asking for it again. */
    lang->count = 0;
    lang->strings.used = 0;

    if (lang->bucket_count > 0)
    {
        size_t i;

        for (i = 0; i < lang->bucket_count; i++)
        {
            lang->buckets[i] = CI18N_NO_INDEX;
        }
    }

    ci18n_succeed(ctx);
    return true;
}
CI18N_DEF bool ci18n_clear(const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_clear_impl(&ci18n_ctx, language_code);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}


static bool ci18n_remove_language_impl(ci18n_t *ctx, const char *language_code)
{
    int lang_idx;
    size_t last;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    lang_idx = ci18n_find_language(ctx, language_code);
    if (lang_idx < 0)
    {
        return ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    /* Pointing at a language that no longer exists would be worse than
     * pointing at nothing, so drop the selection first. */
    if (strcmp(ctx->current_language, language_code) == 0)
    {
        ctx->current_language[0] = '\0';
    }
    if (strcmp(ctx->fallback_language, language_code) == 0)
    {
        ctx->fallback_language[0] = '\0';
    }

    ci18n_lang_release(&ctx->languages[lang_idx]);

    /* Move the last language into the hole. Languages are found by scanning
     * for the code rather than by index, so nothing holds a stale one, and
     * this avoids shifting the rest. */
    last = ctx->language_count - 1;
    if ((size_t)lang_idx != last)
    {
        ctx->languages[lang_idx] = ctx->languages[last];
        memset(&ctx->languages[last], 0, sizeof(ci18n_language_t));
    }

    ctx->language_count--;

    ci18n_succeed(ctx);
    return true;
}
CI18N_DEF bool ci18n_remove_language(const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_remove_language_impl(&ci18n_ctx, language_code);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}


static size_t ci18n_count_impl(ci18n_t *ctx, const char *language_code)
{
    int lang_idx;

    if (!ctx->initialized)
    {
        ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
        return 0;
    }

    if (!language_code)
    {
        ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
        return 0;
    }

    lang_idx = ci18n_find_language(ctx, language_code);
    if (lang_idx < 0)
    {
        ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
        return 0;
    }

    ci18n_succeed(ctx);
    return ctx->languages[lang_idx].count;
}
CI18N_DEF size_t ci18n_count(const char *language_code)
{
    size_t result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_count_impl(&ci18n_ctx, language_code);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}

static size_t ci18n_foreach_impl(ci18n_t *ctx, const char *language_code,
                                 ci18n_entry_fn fn, void *user_data)
{
    const ci18n_language_t *lang;
    int lang_idx;
    size_t i;

    if (!ctx->initialized)
    {
        ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
        return 0;
    }

    if (!language_code || !fn)
    {
        ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
        return 0;
    }

    lang_idx = ci18n_find_language(ctx, language_code);
    if (lang_idx < 0)
    {
        ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
        return 0;
    }

    /* Success is recorded before the walk: the callback cannot reach the
     * library, so nothing it does can change the outcome. */
    ci18n_succeed(ctx);

    lang = &ctx->languages[lang_idx];
    for (i = 0; i < lang->count; i++)
    {
        const ci18n_entry_t *e = &lang->entries[i];

        if (!fn(ci18n_arena_at(&lang->strings, e->key),
                ci18n_arena_at(&lang->strings, e->value), user_data))
        {
            return i + 1;
        }
    }

    return lang->count;
}
CI18N_DEF size_t ci18n_foreach(const char *language_code, ci18n_entry_fn fn,
                               void *user_data)
{
    size_t result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_foreach_impl(&ci18n_ctx, language_code, fn, user_data);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


/* ============================================================================
 * Plurals
 * ============================================================================ */
/* ============================================================================
 * Interpolation
 * ============================================================================ */

/* The unlocked cores, declared here because the formatters sit above them and
 * must not call the locking wrappers: these locks are not recursive. */
static const char *ci18n_get_impl(ci18n_t *ctx, const char *key);
static const char *ci18n_plural_impl(ci18n_t *ctx, const char *key, long count);
#if !defined(CI18N_NO_ORDINALS)
static const char *ci18n_ordinal_impl(ci18n_t *ctx, const char *key, long count);
#endif

/*
 * Writes into the caller's buffer while counting what the whole result would
 * need, so one pass can both fill a buffer and report a too-small one.
 */
/* ============================================================================
 * UTF-8
 *
 * One table drives all of it. For a lead byte it gives the sequence length
 * and the allowed range of the second byte, which is where the awkward cases
 * live: the second byte is what distinguishes an overlong encoding from a
 * real one, and a surrogate half from a legitimate character.
 * ============================================================================ */

static bool ci18n_utf8_is_continuation(unsigned char c)
{
    return (c & 0xC0u) == 0x80u;
}

/*
 * Decode the sequence at `text`, strictly.
 *
 * Returns its length in bytes, or 0 if the bytes there are not a well-formed
 * character. `limit` is how many bytes are readable, so a sequence running
 * past the end is rejected rather than read.
 */
static size_t ci18n_utf8_decode(const unsigned char *text, size_t limit)
{
    unsigned char lead;
    unsigned char second_min = 0x80u;
    unsigned char second_max = 0xBFu;
    size_t length;
    size_t i;

    if (limit == 0)
    {
        return 0;
    }

    lead = text[0];

    if (lead < 0x80u)
    {
        return 1;
    }
    else if (lead >= 0xC2u && lead <= 0xDFu)
    {
        /* 0xC0 and 0xC1 could only ever encode a value that fits in one
         * byte, so they are overlong by construction and never valid. */
        length = 2;
    }
    else if (lead >= 0xE0u && lead <= 0xEFu)
    {
        length = 3;

        if (lead == 0xE0u)
        {
            second_min = 0xA0u; /* below this is an overlong 2-byte value */
        }
        else if (lead == 0xEDu)
        {
            second_max = 0x9Fu; /* above this is a surrogate, U+D800 to U+DFFF */
        }
    }
    else if (lead >= 0xF0u && lead <= 0xF4u)
    {
        length = 4;

        if (lead == 0xF0u)
        {
            second_min = 0x90u; /* below this is an overlong 3-byte value */
        }
        else if (lead == 0xF4u)
        {
            second_max = 0x8Fu; /* above this is beyond U+10FFFF */
        }
    }
    else
    {
        /* A continuation byte with no lead, or 0xF5 and up, which no
         * codepoint reaches. */
        return 0;
    }

    if (limit < length)
    {
        return 0;
    }

    if (text[1] < second_min || text[1] > second_max)
    {
        return 0;
    }

    for (i = 2; i < length; i++)
    {
        if (!ci18n_utf8_is_continuation(text[i]))
        {
            return 0;
        }
    }

    return length;
}

/* The number of bytes the sequence starting with `c` should occupy, or 0 if
 * `c` cannot start one. */
static size_t ci18n_utf8_lead_length(unsigned char c)
{
    if (c < 0x80u)
    {
        return 1;
    }
    if ((c & 0xE0u) == 0xC0u)
    {
        return 2;
    }
    if ((c & 0xF0u) == 0xE0u)
    {
        return 3;
    }
    if ((c & 0xF8u) == 0xF0u)
    {
        return 4;
    }
    return 0;
}

/*
 * How much of text[0 .. len) to keep so it does not end mid character.
 *
 * Only the tail is judged, and only by looking inside that range, so this
 * works on a buffer a formatter has already written into and whose following
 * byte is gone. Invalid bytes are left alone: tidying a cut is this
 * function's job, and rewriting bad data is not.
 */
static size_t ci18n_utf8_trim_partial(const char *text, size_t len)
{
    size_t lead = len;
    size_t steps = 0;
    size_t need;

    /* At most three continuation bytes can follow a lead byte, so a lead byte
     * within four steps is the one that governs the tail. */
    while (lead > 0 && steps < 4 &&
           ci18n_utf8_is_continuation((unsigned char)text[lead - 1]))
    {
        lead--;
        steps++;
    }

    if (lead == 0)
    {
        /* Nothing but continuation bytes in reach, which is not a cut we
         * made. Leave it. */
        return len;
    }

    lead--;
    need = ci18n_utf8_lead_length((unsigned char)text[lead]);

    if (need == 0 || lead + need <= len)
    {
        /* Not a lead byte at all, or its sequence is complete. */
        return len;
    }

    return lead;
}

CI18N_DEF bool ci18n_utf8_valid(const char *text)
{
    const unsigned char *p;
    size_t len;

    if (!text)
    {
        return false;
    }

    p = (const unsigned char *)text;
    len = strlen(text);

    while (len > 0)
    {
        size_t step = ci18n_utf8_decode(p, len);

        if (step == 0)
        {
            return false;
        }

        p += step;
        len -= step;
    }

    return true;
}

CI18N_DEF size_t ci18n_utf8_length(const char *text)
{
    const unsigned char *p;
    size_t len;
    size_t count = 0;

    if (!text)
    {
        return 0;
    }

    p = (const unsigned char *)text;
    len = strlen(text);

    while (len > 0)
    {
        size_t step = ci18n_utf8_decode(p, len);

        /* An invalid byte counts as one character, so the count stays defined
         * and the walk always makes progress. */
        if (step == 0)
        {
            step = 1;
        }

        p += step;
        len -= step;
        count++;
    }

    return count;
}

CI18N_DEF size_t ci18n_utf8_sequence_length(const char *text)
{
    size_t step;

    if (!text || text[0] == '\0')
    {
        return 0;
    }

    step = ci18n_utf8_decode((const unsigned char *)text, strlen(text));

    /* Stepping by 1 past a bad byte beats returning 0 and hanging the
     * caller's loop. */
    return (step == 0) ? 1 : step;
}

CI18N_DEF size_t ci18n_utf8_truncate(char *text, size_t max_bytes)
{
    size_t len;
    size_t cut;

    if (!text)
    {
        return 0;
    }

    len = strlen(text);

    if (len <= max_bytes)
    {
        return len;
    }

    /* Walk back off any continuation bytes, so the cut lands between
     * characters rather than inside one. At most three steps, since that is
     * the longest run of continuation bytes UTF-8 allows. */
    cut = max_bytes;
    while (cut > 0 && ci18n_utf8_is_continuation((unsigned char)text[cut]))
    {
        cut--;
    }

    text[cut] = '\0';
    return cut;
}

typedef struct ci18n_sink
{
    char *out;
    size_t capacity;
    size_t written; /* bytes actually placed, never past capacity - 1 */
    size_t needed;  /* bytes the full result would take */
    bool full;      /* something was cut; later pieces are only counted */
} ci18n_sink_t;

static void ci18n_sink_put(ci18n_sink_t *sink, const char *text, size_t len)
{
    sink->needed += len;

    if (sink->capacity == 0 || sink->full)
    {
        return;
    }

    if (sink->written + len > sink->capacity - 1)
    {
        /* Nothing after this point may be written, even a piece short
         * enough to fit in what is left: the output would then skip a
         * stretch of the text and stop being a prefix of the full result,
         * for example a value's closing PDI without its opening FSI. */
        sink->full = true;
        len = sink->capacity - 1 - sink->written;

        /* Cutting here would land wherever the budget ran out, which for
         * UTF-8 can be halfway through a character. Drop a trailing partial
         * character so a truncated result is still decodable. */
        len = ci18n_utf8_trim_partial(text, len);
    }

    if (len > 0)
    {
        memcpy(sink->out + sink->written, text, len);
        sink->written += len;
    }
}

#if !defined(CI18N_NO_FORMAT)
/*
 * Find a placeholder's value among the name and value pairs.
 *
 * The list is walked from the start for every placeholder, on a copy, since a
 * va_list cannot be rewound. Translations hold a handful of placeholders and
 * callers pass a handful of pairs, so the cost is not worth an index.
 */
static const char *ci18n_lookup_argument(va_list args, const char *name, size_t name_len)
{
    va_list copy;
    const char *found = NULL;

    va_copy(copy, args);

    for (;;)
    {
        const char *arg_name = va_arg(copy, const char *);
        const char *arg_value;

        if (!arg_name)
        {
            break;
        }

        arg_value = va_arg(copy, const char *);

        if (strlen(arg_name) == name_len && strncmp(arg_name, name, name_len) == 0)
        {
            found = arg_value ? arg_value : "";
            break;
        }
    }

    va_end(copy);
    return found;
}

/*
 * Let a formatter write straight into what is left of the output buffer.
 *
 * No intermediate buffer, so nothing bounds the result except the caller's
 * own capacity, and the measuring pass costs nothing. The formatter follows
 * snprintf(), so it reports the full length whether or not it fitted.
 */
static void ci18n_sink_put_formatted(ci18n_sink_t *sink,
                                     const ci18n_formatter_t *formatter,
                                     const char *value, const char *arg)
{
    char *dest = NULL;
    size_t room = 0;
    size_t produced;

    if (sink->capacity > 0 && !sink->full)
    {
        /* Everything left, including the byte the terminator will want, which
         * is exactly snprintf's idea of a capacity. */
        room = sink->capacity - sink->written;
        dest = sink->out + sink->written;
    }

    produced = formatter->fn(dest, room, value, arg, formatter->user_data);

    sink->needed += produced;

    if (room == 0)
    {
        return;
    }

    if (produced > room - 1)
    {
        sink->full = true;
        produced = room - 1;
    }

    sink->written += ci18n_utf8_trim_partial(dest, produced);
}

/* Find a formatter by name. The table is tiny, so a scan is the right shape. */
static const ci18n_formatter_t *ci18n_find_formatter(ci18n_t *ctx, const char *name)
{
    size_t i;

    for (i = 0; i < ctx->formatter_count; i++)
    {
        if (strcmp(ctx->formatters[i].name, name) == 0)
        {
            return &ctx->formatters[i];
        }
    }

    return NULL;
}

/*
 * Copy a bounded piece of a placeholder into `out`.
 *
 * Returns false when it does not fit, which the caller turns into a visible
 * placeholder rather than a quietly shortened one.
 */
static bool ci18n_copy_bounded(char *out, size_t capacity, const char *text, size_t len)
{
    if (len >= capacity)
    {
        return false;
    }

    memcpy(out, text, len);
    out[len] = '\0';
    return true;
}
#endif

/*
 * Expand {placeholders} in `text`.
 *
 * `count_text` is the pre-rendered number for {count}, or NULL when there is
 * no count. An explicit pair of the same name still wins, so a caller can
 * override it.
 *
 * `problem` collects anything wrong with the translation itself, such as a
 * formatter it names that nobody registered. Reporting it here rather than
 * returning early keeps the rest of the sentence intact, which is what a
 * user wants to see.
 */
#if !defined(CI18N_NO_FORMAT) && !defined(CI18N_NO_NUMBERS)
/* The built-in "number" formatter, defined with the number tables below. */
static size_t ci18n_number_formatter(char *out, size_t capacity, const char *value,
                                     const char *arg, void *user_data);
#endif

#if !defined(CI18N_NO_FORMAT)
static size_t ci18n_expand(ci18n_t *ctx, char *out, size_t capacity,
                           const char *text, va_list args,
                           const char *count_text, ci18n_error_t *problem)
{
    ci18n_sink_t sink;
    size_t i = 0;

    sink.out = out;
    sink.capacity = capacity;
    sink.written = 0;
    sink.needed = 0;
    sink.full = false;

    while (text[i] != '\0')
    {
        size_t start;
        size_t body_len;
        size_t name_len;
        size_t spec_len;
        const char *spec;
        const char *value;

        if (text[i] != '{')
        {
            /* Literal run. Copied in one go rather than byte by byte. */
            start = i;
            while (text[i] != '\0' && text[i] != '{' && text[i] != '}')
            {
                i++;
            }

            if (i > start)
            {
                ci18n_sink_put(&sink, text + start, i - start);
                continue;
            }

            /* A lone '}' is literal; '}}' is an escaped one. */
            ci18n_sink_put(&sink, "}", 1);
            i += (text[i + 1] == '}') ? 2 : 1;
            continue;
        }

        /* '{{' is an escaped brace. */
        if (text[i + 1] == '{')
        {
            ci18n_sink_put(&sink, "{", 1);
            i += 2;
            continue;
        }

        start = i + 1;
        body_len = 0;
        while (text[start + body_len] != '\0' && text[start + body_len] != '}')
        {
            body_len++;
        }

        /* Unterminated: the rest of the string is literal, not a placeholder. */
        if (text[start + body_len] != '}')
        {
            ci18n_sink_put(&sink, text + i, strlen(text + i));
            break;
        }

        /* {name}, {name:formatter} or {name:formatter,argument}. */
        name_len = 0;
        while (name_len < body_len && text[start + name_len] != ':')
        {
            name_len++;
        }

        spec = (name_len < body_len) ? text + start + name_len + 1 : NULL;
        spec_len = spec ? body_len - name_len - 1 : 0;

        value = ci18n_lookup_argument(args, text + start, name_len);

        if (!value && count_text &&
            name_len == 5 && strncmp(text + start, "count", 5) == 0)
        {
            value = count_text;
        }

        if (value && spec)
        {
            const ci18n_formatter_t *formatter = NULL;
#ifndef CI18N_NO_NUMBERS
            ci18n_formatter_t builtin;
#endif
            char formatter_name[CI18N_MAX_FORMATTER_NAME];
            char formatter_arg[CI18N_MAX_FORMATTER_ARG];
            size_t select_len = 0;
            bool usable;

            /* The formatter is named up to the first comma; everything after
             * it belongs to the formatter, verbatim. */
            while (select_len < spec_len && spec[select_len] != ',')
            {
                select_len++;
            }

            formatter_arg[0] = '\0';
            usable = ci18n_copy_bounded(formatter_name, sizeof(formatter_name),
                                        spec, select_len);

            if (usable && select_len < spec_len)
            {
                usable = ci18n_copy_bounded(formatter_arg, sizeof(formatter_arg),
                                            spec + select_len + 1,
                                            spec_len - select_len - 1);
            }

            if (!usable)
            {
                /* Longer than anything that could have been registered, or
                 * than an argument is allowed to be: the translation is
                 * malformed rather than the code. */
                *problem = CI18N_ERR_PARSE;
            }
            else
            {
                formatter = ci18n_find_formatter(ctx, formatter_name);

#ifndef CI18N_NO_NUMBERS
                /* Built in, and found last, so a registered one of the same
                 * name wins. */
                if (!formatter && strcmp(formatter_name, "number") == 0)
                {
                    builtin.fn = ci18n_number_formatter;
                    builtin.user_data = ctx->current_language;
                    formatter = &builtin;
                }
#endif

                if (!formatter)
                {
                    *problem = CI18N_ERR_UNKNOWN_FORMATTER;
                }
            }

            if (formatter)
            {
                if (ctx->bidi_isolation)
                {
                    ci18n_sink_put(&sink, CI18N_FSI, 3);
                }
                ci18n_sink_put_formatted(&sink, formatter, value, formatter_arg);
                if (ctx->bidi_isolation)
                {
                    ci18n_sink_put(&sink, CI18N_PDI, 3);
                }
            }
            else
            {
                ci18n_sink_put(&sink, text + i, body_len + 2);
            }
        }
        else if (value)
        {
            /* The isolate goes around the value only, never around text
             * the translator wrote, so a placeholder left visible for a
             * missing formatter stays unwrapped as well. */
            if (ctx->bidi_isolation)
            {
                ci18n_sink_put(&sink, CI18N_FSI, 3);
            }
            ci18n_sink_put(&sink, value, strlen(value));
            if (ctx->bidi_isolation)
            {
                ci18n_sink_put(&sink, CI18N_PDI, 3);
            }
        }
        else
        {
            /* No such name: leave the placeholder visible, so a typo in a
             * translation is something you can see rather than a hole. */
            ci18n_sink_put(&sink, text + i, body_len + 2);
        }

        i = start + body_len + 1;
    }

    if (capacity > 0)
    {
        sink.out[sink.written] = '\0';
    }

    return sink.needed;
}

/* Render a count without pulling in snprintf's format machinery. */
static void ci18n_render_long(char *out, size_t capacity, long value)
{
    char digits[24];
    size_t n = 0;
    size_t i = 0;
    unsigned long magnitude;

    if (capacity == 0)
    {
        return;
    }

    magnitude = (unsigned long)(value < 0 ? -value : value);

    do
    {
        digits[n++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    } while (magnitude > 0 && n < sizeof(digits));

    if (value < 0 && i + 1 < capacity)
    {
        out[i++] = '-';
    }

    while (n > 0 && i + 1 < capacity)
    {
        out[i++] = digits[--n];
    }

    out[i] = '\0';
}


static bool ci18n_set_formatter_impl(ci18n_t *ctx, const char *name,
                                     ci18n_formatter_fn fn, void *user_data)
{
    size_t i;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!name || name[0] == '\0' || !fn)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    if (strlen(name) >= CI18N_MAX_FORMATTER_NAME)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    /* A name carrying the punctuation that selects it could never be matched
     * from a translation, so it is a mistake worth reporting rather than a
     * formatter that silently never runs. */
    if (strpbrk(name, ":,{}") != NULL)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    for (i = 0; i < ctx->formatter_count; i++)
    {
        if (strcmp(ctx->formatters[i].name, name) == 0)
        {
            ctx->formatters[i].fn = fn;
            ctx->formatters[i].user_data = user_data;
            ci18n_succeed(ctx);
            return true;
        }
    }

    if (ctx->formatter_count >= CI18N_MAX_FORMATTERS)
    {
        return ci18n_fail(ctx, CI18N_ERR_TOO_MANY_FORMATTERS);
    }

    ci18n_copy(ctx->formatters[ctx->formatter_count].name,
               sizeof(ctx->formatters[0].name), name);
    ctx->formatters[ctx->formatter_count].fn = fn;
    ctx->formatters[ctx->formatter_count].user_data = user_data;
    ctx->formatter_count++;

    ci18n_succeed(ctx);
    return true;
}

static bool ci18n_remove_formatter_impl(ci18n_t *ctx, const char *name)
{
    size_t i;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!name)
    {
        return ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
    }

    for (i = 0; i < ctx->formatter_count; i++)
    {
        if (strcmp(ctx->formatters[i].name, name) != 0)
        {
            continue;
        }

        /* Move the last entry into the hole, as the entry table does. Order
         * is not observable: lookup is by name. */
        ctx->formatter_count--;
        if (i != ctx->formatter_count)
        {
            ctx->formatters[i] = ctx->formatters[ctx->formatter_count];
        }
        memset(&ctx->formatters[ctx->formatter_count], 0,
               sizeof(ctx->formatters[0]));

        ci18n_succeed(ctx);
        return true;
    }

    return ci18n_fail(ctx, CI18N_ERR_KEY_NOT_FOUND);
}

CI18N_DEF bool ci18n_set_formatter_in(ci18n_t *catalog, const char *name,
                                      ci18n_formatter_fn fn, void *user_data)
{
    bool result;

    if (!catalog)
    {
        return false;
    }

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_set_formatter_impl(catalog, name, fn, user_data);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_set_formatter(const char *name, ci18n_formatter_fn fn,
                                   void *user_data)
{
    return ci18n_set_formatter_in(&ci18n_ctx, name, fn, user_data);
}

CI18N_DEF bool ci18n_remove_formatter_in(ci18n_t *catalog, const char *name)
{
    bool result;

    if (!catalog)
    {
        return false;
    }

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_remove_formatter_impl(catalog, name);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_remove_formatter(const char *name)
{
    return ci18n_remove_formatter_in(&ci18n_ctx, name);
}

/*
 * Expand a translation and report anything the translation itself got wrong,
 * such as naming a formatter nobody registered. The error is raised after the
 * whole string is expanded rather than at the first problem, so the rest of
 * the sentence still comes out.
 */
static size_t ci18n_expand_reporting(ci18n_t *ctx, char *out, size_t capacity,
                                     const char *text, va_list args,
                                     const char *count_text)
{
    ci18n_error_t problem = CI18N_OK;
    size_t needed;

    needed = ci18n_expand(ctx, out, out ? capacity : 0, text, args,
                          count_text, &problem);

    if (problem == CI18N_OK)
    {
        ci18n_succeed(ctx);
    }
    else
    {
        (void)ci18n_fail(ctx, problem);
    }

    return needed;
}

CI18N_DEF size_t ci18n_format_in(ci18n_t *catalog, char *out, size_t capacity,
                                 const char *key, ...)
{
    va_list args;
    const char *text;
    size_t needed;

    if (capacity > 0 && out)
    {
        out[0] = '\0';
    }

    if (!catalog)
    {
        return 0;
    }

    CI18N_READ_LOCK(catalog);

    text = ci18n_get_impl(catalog, key);
    if (!text)
    {
        CI18N_READ_UNLOCK(catalog);
        return 0;
    }

    va_start(args, key);
    needed = ci18n_expand_reporting(catalog, out, capacity, text, args, NULL);
    va_end(args);

    CI18N_READ_UNLOCK(catalog);
    return needed;
}

CI18N_DEF size_t ci18n_format(char *out, size_t capacity, const char *key, ...)
{
    va_list args;
    const char *text;
    size_t needed;

    if (capacity > 0 && out)
    {
        out[0] = '\0';
    }

    CI18N_READ_LOCK(&ci18n_ctx);

    text = ci18n_get_impl(&ci18n_ctx, key);
    if (!text)
    {
        CI18N_READ_UNLOCK(&ci18n_ctx);
        return 0;
    }

    va_start(args, key);
    needed = ci18n_expand_reporting(&ci18n_ctx, out, capacity, text, args, NULL);
    va_end(args);

    CI18N_READ_UNLOCK(&ci18n_ctx);
    return needed;
}

CI18N_DEF size_t ci18n_format_plural_in(ci18n_t *catalog, char *out, size_t capacity,
                                        const char *key, long count, ...)
{
    va_list args;
    const char *text;
    char count_text[24];
    size_t needed;

    if (capacity > 0 && out)
    {
        out[0] = '\0';
    }

    if (!catalog)
    {
        return 0;
    }

    CI18N_READ_LOCK(catalog);

    text = ci18n_plural_impl(catalog, key, count);
    if (!text)
    {
        CI18N_READ_UNLOCK(catalog);
        return 0;
    }

    ci18n_render_long(count_text, sizeof(count_text), count);

    va_start(args, count);
    needed = ci18n_expand_reporting(catalog, out, capacity, text, args, count_text);
    va_end(args);

    CI18N_READ_UNLOCK(catalog);
    return needed;
}

CI18N_DEF size_t ci18n_format_plural(char *out, size_t capacity, const char *key,
                                     long count, ...)
{
    va_list args;
    const char *text;
    char count_text[24];
    size_t needed;

    if (capacity > 0 && out)
    {
        out[0] = '\0';
    }

    CI18N_READ_LOCK(&ci18n_ctx);

    text = ci18n_plural_impl(&ci18n_ctx, key, count);
    if (!text)
    {
        CI18N_READ_UNLOCK(&ci18n_ctx);
        return 0;
    }

    ci18n_render_long(count_text, sizeof(count_text), count);

    va_start(args, count);
    needed = ci18n_expand_reporting(&ci18n_ctx, out, capacity, text, args, count_text);
    va_end(args);

    CI18N_READ_UNLOCK(&ci18n_ctx);
    return needed;
}
#endif

#if !defined(CI18N_NO_FORMAT) && !defined(CI18N_NO_ORDINALS)
CI18N_DEF size_t ci18n_format_ordinal_in(ci18n_t *catalog, char *out, size_t capacity,
                                         const char *key, long count, ...)
{
    va_list args;
    const char *text;
    char count_text[24];
    size_t needed;

    if (capacity > 0 && out)
    {
        out[0] = '\0';
    }

    if (!catalog)
    {
        return 0;
    }

    CI18N_READ_LOCK(catalog);

    text = ci18n_ordinal_impl(catalog, key, count);
    if (!text)
    {
        CI18N_READ_UNLOCK(catalog);
        return 0;
    }

    ci18n_render_long(count_text, sizeof(count_text), count);

    va_start(args, count);
    needed = ci18n_expand_reporting(catalog, out, capacity, text, args, count_text);
    va_end(args);

    CI18N_READ_UNLOCK(catalog);
    return needed;
}

CI18N_DEF size_t ci18n_format_ordinal(char *out, size_t capacity, const char *key,
                                      long count, ...)
{
    va_list args;
    const char *text;
    char count_text[24];
    size_t needed;

    if (capacity > 0 && out)
    {
        out[0] = '\0';
    }

    CI18N_READ_LOCK(&ci18n_ctx);

    text = ci18n_ordinal_impl(&ci18n_ctx, key, count);
    if (!text)
    {
        CI18N_READ_UNLOCK(&ci18n_ctx);
        return 0;
    }

    ci18n_render_long(count_text, sizeof(count_text), count);

    va_start(args, count);
    needed = ci18n_expand_reporting(&ci18n_ctx, out, capacity, text, args, count_text);
    va_end(args);

    CI18N_READ_UNLOCK(&ci18n_ctx);
    return needed;
}
#endif


/*
 * CLDR groups languages by the plural rule they follow, so the rules live
 * here as families and the table below maps languages onto them. Writing out
 * one rule per language would be a few hundred near-duplicates.
 */
typedef enum ci18n_plural_family
{
    CI18N_PF_OTHER_ONLY,  /* ja, zh, ko: no plural distinction at all */
    CI18N_PF_ONE_OTHER,   /* en, de, es: one for exactly 1 */
    CI18N_PF_ZERO_ONE,    /* hi, bn, si: 0 counts as one too */
    CI18N_PF_FRENCH,      /* fr, pt: 0 counts as one, and whole millions are many */
    CI18N_PF_ROMANCE,     /* es, it, ca: one for 1, and whole millions are many */
    CI18N_PF_SLAVIC,      /* ru, uk, be */
    CI18N_PF_POLISH,      /* pl */
    CI18N_PF_CZECH,       /* cs, sk */
    CI18N_PF_BALKAN,      /* hr, sr, bs */
    CI18N_PF_ARABIC,      /* ar */
    CI18N_PF_LITHUANIAN,  /* lt */
    CI18N_PF_LATVIAN,     /* lv */
    CI18N_PF_SLOVENIAN,   /* sl */
    CI18N_PF_IRISH,       /* ga */
    CI18N_PF_ROMANIAN,    /* ro */
    CI18N_PF_HEBREW       /* he: a dual, so two is its own form */
} ci18n_plural_family_t;

typedef struct ci18n_plural_rule
{
    const char *language;
    ci18n_plural_family_t family;
} ci18n_plural_rule_t;

/*
 * Languages by primary subtag. Not exhaustive, and deliberately so: the point
 * is to cover what people actually translate into, and to fall back to the
 * English rule rather than pretend.
 */
static const ci18n_plural_rule_t ci18n_plural_rules[] = {
    /* No plural distinction. */
    {"ja", CI18N_PF_OTHER_ONLY}, {"zh", CI18N_PF_OTHER_ONLY},
    {"ko", CI18N_PF_OTHER_ONLY}, {"vi", CI18N_PF_OTHER_ONLY},
    {"th", CI18N_PF_OTHER_ONLY}, {"id", CI18N_PF_OTHER_ONLY},
    {"ms", CI18N_PF_OTHER_ONLY}, {"lo", CI18N_PF_OTHER_ONLY},
    {"my", CI18N_PF_OTHER_ONLY}, {"km", CI18N_PF_OTHER_ONLY},
    {"yo", CI18N_PF_OTHER_ONLY}, {"ig", CI18N_PF_OTHER_ONLY},

    /* Zero behaves like one. */
    {"fr", CI18N_PF_FRENCH}, {"pt", CI18N_PF_FRENCH},
    {"es", CI18N_PF_ROMANCE}, {"it", CI18N_PF_ROMANCE}, {"ca", CI18N_PF_ROMANCE},
    {"hi", CI18N_PF_ZERO_ONE}, {"bn", CI18N_PF_ZERO_ONE},
    {"fa", CI18N_PF_ZERO_ONE}, {"hy", CI18N_PF_ZERO_ONE},
    {"gu", CI18N_PF_ZERO_ONE}, {"kn", CI18N_PF_ZERO_ONE},
    {"zu", CI18N_PF_ZERO_ONE},

    /* Three forms, east Slavic. */
    {"ru", CI18N_PF_SLAVIC}, {"uk", CI18N_PF_SLAVIC}, {"be", CI18N_PF_SLAVIC},

    {"pl", CI18N_PF_POLISH},
    {"cs", CI18N_PF_CZECH}, {"sk", CI18N_PF_CZECH},
    {"hr", CI18N_PF_BALKAN}, {"sr", CI18N_PF_BALKAN}, {"bs", CI18N_PF_BALKAN},
    {"ar", CI18N_PF_ARABIC},
    {"lt", CI18N_PF_LITHUANIAN},
    {"lv", CI18N_PF_LATVIAN},
    {"sl", CI18N_PF_SLOVENIAN},
    {"ga", CI18N_PF_IRISH},
    {"ro", CI18N_PF_ROMANIAN},
    {"he", CI18N_PF_HEBREW}, {"iw", CI18N_PF_HEBREW},

    /* One for exactly 1. The default, so these are here for documentation as
     * much as for lookup. */
    {"en", CI18N_PF_ONE_OTHER}, {"de", CI18N_PF_ONE_OTHER},
    {"nl", CI18N_PF_ONE_OTHER}, {"sv", CI18N_PF_ONE_OTHER},
    {"da", CI18N_PF_ONE_OTHER}, {"no", CI18N_PF_ONE_OTHER},
    {"nb", CI18N_PF_ONE_OTHER}, {"nn", CI18N_PF_ONE_OTHER},
    {"fi", CI18N_PF_ONE_OTHER}, {"et", CI18N_PF_ONE_OTHER},
    {"el", CI18N_PF_ONE_OTHER},
    {"hu", CI18N_PF_ONE_OTHER},
    {"bg", CI18N_PF_ONE_OTHER}, {"sq", CI18N_PF_ONE_OTHER},
    {"ka", CI18N_PF_ONE_OTHER}, {"eu", CI18N_PF_ONE_OTHER},
    {"tr", CI18N_PF_ONE_OTHER}, {"az", CI18N_PF_ONE_OTHER},
    {"kk", CI18N_PF_ONE_OTHER}, {"uz", CI18N_PF_ONE_OTHER},
    {"ky", CI18N_PF_ONE_OTHER}, {"mn", CI18N_PF_ONE_OTHER},
    {"ne", CI18N_PF_ONE_OTHER}, {"sw", CI18N_PF_ONE_OTHER},
    {"af", CI18N_PF_ONE_OTHER}, {"mr", CI18N_PF_ONE_OTHER},
    {"ta", CI18N_PF_ONE_OTHER}, {"te", CI18N_PF_ONE_OTHER},
    {"ml", CI18N_PF_ONE_OTHER}, {"si", CI18N_PF_ZERO_ONE},
    {"ur", CI18N_PF_ONE_OTHER}
};

/* Length of the primary subtag, the part before any '-', '_' or '.'. */
static size_t ci18n_primary_subtag_len(const char *code)
{
    size_t i = 0;

    while (code[i] != '\0' && code[i] != '-' && code[i] != '_' && code[i] != '.')
    {
        i++;
    }

    return i;
}

/*
 * ASCII only, on purpose. The ctype functions depend on the C locale, which
 * the host program owns and may have changed, and they are undefined for a
 * negative char. Language tags are ASCII by definition, so this is enough.
 */
static char ci18n_ascii_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

static bool ci18n_ascii_alpha(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

/*
 * Match a NUL-terminated tag against the first `len` bytes of a locale
 * string, ignoring case. BCP 47 subtags are case-insensitive, so "ru", "RU"
 * and "Ru" are one language, and "arab" is the "Arab" script.
 */
static bool ci18n_subtag_eq(const char *tag, const char *code, size_t len)
{
    size_t i;

    if (strlen(tag) != len)
    {
        return false;
    }

    for (i = 0; i < len; i++)
    {
        if (ci18n_ascii_lower(tag[i]) != ci18n_ascii_lower(code[i]))
        {
            return false;
        }
    }

    return true;
}

/*
 * The primary subtag folded to lower case, when it is two letters long.
 *
 * Every language in the rule tables has a two-letter code, so this lets the
 * lookups compare two bytes per row instead of measuring and folding each
 * row again. That made ci18n_plural_category() six times faster.
 * Returns false for any other length, and callers then take the general
 * path, so a three-letter code added to a table later is still found.
 */
static bool ci18n_primary_pair(const char *code, char *first, char *second)
{
    if (ci18n_primary_subtag_len(code) != 2)
    {
        return false;
    }

    *first = ci18n_ascii_lower(code[0]);
    *second = ci18n_ascii_lower(code[1]);
    return true;
}

/* True when a lower-case table tag is exactly the two letters given. */
static bool ci18n_tag_is_pair(const char *tag, char first, char second)
{
    return tag[0] == first && tag[1] == second && tag[2] == '\0';
}

/*
 * The script subtag, if the code has one.
 *
 * It is the second subtag, and only when that is exactly four letters:
 * "az-Arab" has one, "az-AZ" has a region instead, and "de-1996" a variant.
 * Returns NULL when there is none, leaving *out_len untouched.
 */
static const char *ci18n_script_subtag(const char *code, size_t *out_len)
{
    size_t start = ci18n_primary_subtag_len(code);
    size_t n = 0;

    if (code[start] != '-' && code[start] != '_')
    {
        return NULL;
    }
    start++;

    while (ci18n_ascii_alpha(code[start + n]))
    {
        n++;
    }

    if (n != 4)
    {
        return NULL;
    }

    /* Four letters, and then the subtag has to actually end: "Arabic" is not
     * the "Arab" script. */
    if (code[start + n] != '\0' && code[start + n] != '-' &&
        code[start + n] != '_' && code[start + n] != '.')
    {
        return NULL;
    }

    *out_len = n;
    return code + start;
}

static ci18n_plural_family_t ci18n_plural_family(const char *language_code)
{
    size_t len;
    size_t i;
    char first, second;

    if (!language_code)
    {
        return CI18N_PF_ONE_OTHER;
    }

    if (ci18n_primary_pair(language_code, &first, &second))
    {
        for (i = 0; i < sizeof(ci18n_plural_rules) / sizeof(ci18n_plural_rules[0]); i++)
        {
            if (ci18n_tag_is_pair(ci18n_plural_rules[i].language, first, second))
            {
                return ci18n_plural_rules[i].family;
            }
        }
        return CI18N_PF_ONE_OTHER;
    }

    len = ci18n_primary_subtag_len(language_code);

    for (i = 0; i < sizeof(ci18n_plural_rules) / sizeof(ci18n_plural_rules[0]); i++)
    {
        const char *candidate = ci18n_plural_rules[i].language;

        if (ci18n_subtag_eq(candidate, language_code, len))
        {
            return ci18n_plural_rules[i].family;
        }
    }

    /* Unknown language: guess the commonest rule rather than refuse. */
    return CI18N_PF_ONE_OTHER;
}

CI18N_DEF ci18n_plural_category_t ci18n_plural_category(const char *language_code, long count)
{
    /* Rules are written in terms of the absolute value; a negative count of
     * things is still that many things. */
    unsigned long n = (unsigned long)(count < 0 ? -count : count);
    unsigned long mod10 = n % 10;
    unsigned long mod100 = n % 100;

    switch (ci18n_plural_family(language_code))
    {
    case CI18N_PF_OTHER_ONLY:
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_ZERO_ONE:
        return (n == 0 || n == 1) ? CI18N_PLURAL_ONE : CI18N_PLURAL_OTHER;

    /* A whole number of millions takes its own form in these, because the
     * noun is joined by a preposition: "1 000 000 de fichiers", not
     * "1 000 000 fichiers". A translation that never provides [many] still
     * works, since lookup falls back to [other]. */
    case CI18N_PF_FRENCH:
        if (n == 0 || n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        return (n % 1000000 == 0) ? CI18N_PLURAL_MANY : CI18N_PLURAL_OTHER;

    case CI18N_PF_ROMANCE:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        return (n != 0 && n % 1000000 == 0) ? CI18N_PLURAL_MANY : CI18N_PLURAL_OTHER;

    case CI18N_PF_SLAVIC:
        if (mod10 == 1 && mod100 != 11)
        {
            return CI18N_PLURAL_ONE;
        }
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_MANY;

    case CI18N_PF_POLISH:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_MANY;

    case CI18N_PF_CZECH:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n >= 2 && n <= 4)
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_BALKAN:
        if (mod10 == 1 && mod100 != 11)
        {
            return CI18N_PLURAL_ONE;
        }
        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14))
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_ARABIC:
        if (n == 0)
        {
            return CI18N_PLURAL_ZERO;
        }
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 2)
        {
            return CI18N_PLURAL_TWO;
        }
        if (mod100 >= 3 && mod100 <= 10)
        {
            return CI18N_PLURAL_FEW;
        }
        if (mod100 >= 11 && mod100 <= 99)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_LITHUANIAN:
        if (mod10 == 1 && (mod100 < 11 || mod100 > 19))
        {
            return CI18N_PLURAL_ONE;
        }
        if (mod10 >= 2 && mod10 <= 9 && (mod100 < 11 || mod100 > 19))
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_LATVIAN:
        if (mod10 == 0 || (mod100 >= 11 && mod100 <= 19))
        {
            return CI18N_PLURAL_ZERO;
        }
        if (mod10 == 1 && mod100 != 11)
        {
            return CI18N_PLURAL_ONE;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_SLOVENIAN:
        if (mod100 == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (mod100 == 2)
        {
            return CI18N_PLURAL_TWO;
        }
        if (mod100 == 3 || mod100 == 4)
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_IRISH:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 2)
        {
            return CI18N_PLURAL_TWO;
        }
        if (n >= 3 && n <= 6)
        {
            return CI18N_PLURAL_FEW;
        }
        if (n >= 7 && n <= 10)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_ROMANIAN:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 0 || (mod100 >= 1 && mod100 <= 19))
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_PF_HEBREW:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        return (n == 2) ? CI18N_PLURAL_TWO : CI18N_PLURAL_OTHER;

    case CI18N_PF_ONE_OTHER:
    default:
        return (n == 1) ? CI18N_PLURAL_ONE : CI18N_PLURAL_OTHER;
    }
}


#if !defined(CI18N_NO_ORDINALS)
/*
 * Ordinal rules, CLDR 48, grouped the same way as the cardinal ones. Only the
 * languages that have something other than "other" are listed: everything
 * else, known or not, takes the default.
 */
typedef enum ci18n_ordinal_family
{
    CI18N_OF_OTHER_ONLY,  /* ru, de, es, ja, pl and most others */
    CI18N_OF_FIRST,       /* fr, ga, hy, lo, ms, ro, vi: only 1 differs */
    CI18N_OF_ENGLISH,     /* en: 1st 2nd 3rd, but 11th 12th 13th */
    CI18N_OF_SWEDISH,     /* sv */
    CI18N_OF_ITALIAN,     /* it */
    CI18N_OF_HUNGARIAN,   /* hu */
    CI18N_OF_ALBANIAN,    /* sq */
    CI18N_OF_UKRAINIAN,   /* uk */
    CI18N_OF_BELARUSIAN,  /* be */
    CI18N_OF_KAZAKH,      /* kk */
    CI18N_OF_NEPALI,      /* ne */
    CI18N_OF_CATALAN,     /* ca */
    CI18N_OF_HINDI,       /* hi, gu */
    CI18N_OF_BENGALI,     /* bn */
    CI18N_OF_MARATHI,     /* mr */
    CI18N_OF_GEORGIAN,    /* ka */
    CI18N_OF_AZERBAIJANI  /* az */
} ci18n_ordinal_family_t;

typedef struct ci18n_ordinal_rule
{
    const char *language;
    ci18n_ordinal_family_t family;
} ci18n_ordinal_rule_t;

static const ci18n_ordinal_rule_t ci18n_ordinal_rules[] = {
    {"fr", CI18N_OF_FIRST}, {"ga", CI18N_OF_FIRST}, {"hy", CI18N_OF_FIRST},
    {"lo", CI18N_OF_FIRST}, {"ms", CI18N_OF_FIRST}, {"ro", CI18N_OF_FIRST},
    {"vi", CI18N_OF_FIRST},

    {"en", CI18N_OF_ENGLISH},
    {"sv", CI18N_OF_SWEDISH},
    {"it", CI18N_OF_ITALIAN},
    {"hu", CI18N_OF_HUNGARIAN},
    {"sq", CI18N_OF_ALBANIAN},
    {"uk", CI18N_OF_UKRAINIAN},
    {"be", CI18N_OF_BELARUSIAN},
    {"kk", CI18N_OF_KAZAKH},
    {"ne", CI18N_OF_NEPALI},
    {"ca", CI18N_OF_CATALAN},
    {"hi", CI18N_OF_HINDI}, {"gu", CI18N_OF_HINDI},
    {"bn", CI18N_OF_BENGALI},
    {"mr", CI18N_OF_MARATHI},
    {"ka", CI18N_OF_GEORGIAN},
    {"az", CI18N_OF_AZERBAIJANI}
};

static ci18n_ordinal_family_t ci18n_ordinal_family(const char *language_code)
{
    size_t len;
    size_t i;
    char first, second;

    if (!language_code)
    {
        return CI18N_OF_OTHER_ONLY;
    }

    if (ci18n_primary_pair(language_code, &first, &second))
    {
        for (i = 0; i < sizeof(ci18n_ordinal_rules) / sizeof(ci18n_ordinal_rules[0]); i++)
        {
            if (ci18n_tag_is_pair(ci18n_ordinal_rules[i].language, first, second))
            {
                return ci18n_ordinal_rules[i].family;
            }
        }
        return CI18N_OF_OTHER_ONLY;
    }

    len = ci18n_primary_subtag_len(language_code);

    for (i = 0; i < sizeof(ci18n_ordinal_rules) / sizeof(ci18n_ordinal_rules[0]); i++)
    {
        if (ci18n_subtag_eq(ci18n_ordinal_rules[i].language, language_code, len))
        {
            return ci18n_ordinal_rules[i].family;
        }
    }

    return CI18N_OF_OTHER_ONLY;
}

CI18N_DEF ci18n_plural_category_t ci18n_ordinal_category(const char *language_code, long count)
{
    /* As with cardinals, the rules are written for the magnitude. There is
     * no negative-first place, but a caller passing one gets a sane form. */
    unsigned long n = (unsigned long)(count < 0 ? -count : count);
    unsigned long mod10 = n % 10;
    unsigned long mod100 = n % 100;

    /* No default: every family is handled, and a compiler warning for a
     * missing case is the check that keeps it that way. */
    switch (ci18n_ordinal_family(language_code))
    {
    case CI18N_OF_OTHER_ONLY:
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_FIRST:
        return (n == 1) ? CI18N_PLURAL_ONE : CI18N_PLURAL_OTHER;

    case CI18N_OF_ENGLISH:
        if (mod10 == 1 && mod100 != 11)
        {
            return CI18N_PLURAL_ONE;
        }
        if (mod10 == 2 && mod100 != 12)
        {
            return CI18N_PLURAL_TWO;
        }
        if (mod10 == 3 && mod100 != 13)
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_SWEDISH:
        if ((mod10 == 1 || mod10 == 2) && mod100 != 11 && mod100 != 12)
        {
            return CI18N_PLURAL_ONE;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_ITALIAN:
        if (n == 8 || n == 11 || n == 80 || n == 800)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_HUNGARIAN:
        return (n == 1 || n == 5) ? CI18N_PLURAL_ONE : CI18N_PLURAL_OTHER;

    case CI18N_OF_ALBANIAN:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (mod10 == 4 && mod100 != 14)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_UKRAINIAN:
        if (mod10 == 3 && mod100 != 13)
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_BELARUSIAN:
        if ((mod10 == 2 || mod10 == 3) && mod100 != 12 && mod100 != 13)
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_KAZAKH:
        /* CLDR writes "n % 10 = 6 or n % 10 = 9 or n % 10 = 0 and n != 0",
         * where "and" binds tighter than "or". */
        if (mod10 == 6 || mod10 == 9 || (mod10 == 0 && n != 0))
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_NEPALI:
        return (n >= 1 && n <= 4) ? CI18N_PLURAL_ONE : CI18N_PLURAL_OTHER;

    case CI18N_OF_CATALAN:
        if (n == 1 || n == 3)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 2)
        {
            return CI18N_PLURAL_TWO;
        }
        if (n == 4)
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_HINDI:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 2 || n == 3)
        {
            return CI18N_PLURAL_TWO;
        }
        if (n == 4)
        {
            return CI18N_PLURAL_FEW;
        }
        if (n == 6)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_BENGALI:
        /* Hindi's shape, except that 5 and 7 to 10 join 1 as "one". */
        if (n == 1 || n == 5 || (n >= 7 && n <= 10))
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 2 || n == 3)
        {
            return CI18N_PLURAL_TWO;
        }
        if (n == 4)
        {
            return CI18N_PLURAL_FEW;
        }
        if (n == 6)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_MARATHI:
        /* Hindi's shape without a form for 6. */
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 2 || n == 3)
        {
            return CI18N_PLURAL_TWO;
        }
        if (n == 4)
        {
            return CI18N_PLURAL_FEW;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_GEORGIAN:
        if (n == 1)
        {
            return CI18N_PLURAL_ONE;
        }
        if (n == 0 || (mod100 >= 2 && mod100 <= 20) ||
            mod100 == 40 || mod100 == 60 || mod100 == 80)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;

    case CI18N_OF_AZERBAIJANI:
        if (mod10 == 1 || mod10 == 2 || mod10 == 5 || mod10 == 7 || mod10 == 8 ||
            mod100 == 20 || mod100 == 50 || mod100 == 70 || mod100 == 80)
        {
            return CI18N_PLURAL_ONE;
        }
        /* "i % 1000 = 100,200,...,900" is a whole hundred that is not a
         * whole thousand. */
        if (mod10 == 3 || mod10 == 4 || (mod100 == 0 && n % 1000 != 0))
        {
            return CI18N_PLURAL_FEW;
        }
        if (n == 0 || mod10 == 6 || mod100 == 40 || mod100 == 60 || mod100 == 90)
        {
            return CI18N_PLURAL_MANY;
        }
        return CI18N_PLURAL_OTHER;
    }

    return CI18N_PLURAL_OTHER;
}
#endif

CI18N_DEF const char *ci18n_plural_category_name(ci18n_plural_category_t category)
{
    switch (category)
    {
    case CI18N_PLURAL_ZERO:
        return "zero";
    case CI18N_PLURAL_ONE:
        return "one";
    case CI18N_PLURAL_TWO:
        return "two";
    case CI18N_PLURAL_FEW:
        return "few";
    case CI18N_PLURAL_MANY:
        return "many";
    case CI18N_PLURAL_OTHER:
        return "other";
    }

    return "other";
}


/*
 * The right-to-left scripts, as ISO 15924 codes.
 *
 * Living scripts only. The historic ones (Phoenician, Avestan, Old Turkic and
 * a few dozen more) are also right to left, but a translation file in them is
 * not a case worth carrying a table for.
 */
static const char *const ci18n_rtl_scripts[] = {
    "Arab", /* Arabic */
    "Hebr", /* Hebrew */
    "Syrc", /* Syriac */
    "Thaa", /* Thaana, used for Divehi */
    "Nkoo", /* N'Ko */
    "Adlm", /* Adlam, used for Fula */
    "Mand", /* Mandaic */
    "Samr", /* Samaritan */
    "Rohg", /* Hanifi Rohingya */
    "Yezi"  /* Yezidi */
};

/*
 * Languages whose default script is right to left, by primary subtag.
 *
 * Only languages that are written right to left unless told otherwise belong
 * here. Kurdish is the instructive omission: "ku" is Latin script and so left
 * to right, while Sorani is its own code, "ckb", and is right to left. Same
 * story for "az" and "pa", which are left to right until a script subtag says
 * "az-Arab" or "pa-Arab".
 */
static const char *const ci18n_rtl_languages[] = {
    /* Arabic script. */
    "ar",  /* Arabic */
    "fa",  /* Persian */
    "prs", /* Dari */
    "ur",  /* Urdu */
    "ps",  /* Pashto */
    "ckb", /* Central Kurdish, Sorani */
    "sd",  /* Sindhi */
    "ug",  /* Uyghur */
    "ks",  /* Kashmiri */
    "mzn", /* Mazanderani */
    "glk", /* Gilaki */
    "lrc", /* Northern Luri */

    /* Hebrew script. */
    "he", /* Hebrew */
    "iw", /* Hebrew, the pre-1989 code, still seen in the wild */
    "yi", /* Yiddish */
    "ji", /* Yiddish, likewise */

    /* Aramaic and its descendants. */
    "arc", /* Imperial Aramaic */
    "syr", /* Syriac */
    "sam", /* Samaritan Aramaic */

    /* Elsewhere. */
    "dv", /* Divehi, Thaana script */
    "nqo" /* N'Ko */
};

CI18N_DEF ci18n_direction_t ci18n_direction(const char *language_code)
{
    const char *script;
    size_t script_len = 0;
    size_t len;
    size_t i;

    if (!language_code)
    {
        return CI18N_DIR_LTR;
    }

    /* A script subtag settles it by itself: direction belongs to the script,
     * and an explicit script overrides whatever the language usually uses. */
    script = ci18n_script_subtag(language_code, &script_len);
    if (script)
    {
        for (i = 0; i < sizeof(ci18n_rtl_scripts) / sizeof(ci18n_rtl_scripts[0]); i++)
        {
            if (ci18n_subtag_eq(ci18n_rtl_scripts[i], script, script_len))
            {
                return CI18N_DIR_RTL;
            }
        }

        return CI18N_DIR_LTR;
    }

    len = ci18n_primary_subtag_len(language_code);

    for (i = 0; i < sizeof(ci18n_rtl_languages) / sizeof(ci18n_rtl_languages[0]); i++)
    {
        if (ci18n_subtag_eq(ci18n_rtl_languages[i], language_code, len))
        {
            return CI18N_DIR_RTL;
        }
    }

    return CI18N_DIR_LTR;
}

CI18N_DEF const char *ci18n_direction_name(ci18n_direction_t direction)
{
    return (direction == CI18N_DIR_RTL) ? "rtl" : "ltr";
}

CI18N_DEF ci18n_direction_t ci18n_current_direction_in(ci18n_t *catalog)
{
    ci18n_direction_t result;

    CI18N_READ_LOCK(catalog);
    /* ci18n_direction only reads the code, so computing it under the lock
     * means the pointer never outlives it. */
    result = ci18n_direction(ci18n_get_current_impl(catalog));
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF ci18n_direction_t ci18n_current_direction(void)
{
    return ci18n_current_direction_in(&ci18n_ctx);
}

#if !defined(CI18N_NO_FORMAT)
static bool ci18n_set_bidi_isolation_impl(ci18n_t *ctx, bool enabled)
{
    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    ctx->bidi_isolation = enabled;
    ci18n_succeed(ctx);
    return true;
}

CI18N_DEF bool ci18n_set_bidi_isolation_in(ci18n_t *catalog, bool enabled)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_set_bidi_isolation_impl(catalog, enabled);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_set_bidi_isolation(bool enabled)
{
    return ci18n_set_bidi_isolation_in(&ci18n_ctx, enabled);
}
#endif

CI18N_DEF size_t ci18n_bidi_isolate(char *out, size_t capacity, const char *text)
{
    ci18n_sink_t sink;

    if (out && capacity > 0)
    {
        out[0] = '\0';
    }

    if (!text)
    {
        return 0;
    }

    sink.out = out;
    sink.capacity = out ? capacity : 0;
    sink.written = 0;
    sink.needed = 0;
    sink.full = false;

    ci18n_sink_put(&sink, CI18N_FSI, 3);
    ci18n_sink_put(&sink, text, strlen(text));
    ci18n_sink_put(&sink, CI18N_PDI, 3);

    if (sink.capacity > 0)
    {
        out[sink.written] = '\0';
    }
    return sink.needed;
}

CI18N_DEF const char *ci18n_bidi_mark(ci18n_direction_t direction)
{
    return (direction == CI18N_DIR_RTL) ? CI18N_RLM : CI18N_LRM;
}

/* ============================================================================
 * Numbers
 * ============================================================================ */

#if !defined(CI18N_NO_NUMBERS)
typedef struct ci18n_number_shape
{
    const char *decimal;
    const char *group;
    const char *minus;
    unsigned char min_grouping; /* digits past the first group before any grouping */
    unsigned char secondary;    /* size of groups after the first: 3, or 2 in India */
} ci18n_number_shape_t;

typedef struct ci18n_number_language
{
    char code[4];
    unsigned char shape;
} ci18n_number_language_t;

/* BEGIN generated by tools/cldr_numbers.py, do not edit */
/* CLDR 48.0.0, the Latin-digit symbols of each language. */
static const ci18n_number_shape_t ci18n_number_shapes[] = {
    {",", "\302\240", "-", 1, 3},
    {".", ",", "\342\200\216-", 1, 3},
    {",", ".", "-", 1, 3},
    {",", "\302\240", "-", 2, 3},
    {".", ",", "-", 1, 2},
    {".", ",", "-", 1, 3},
    {",", ".", "-", 2, 3},
    {",", "\302\240", "\342\210\222", 2, 3},
    {",", ".", "\342\210\222", 1, 3},
    {".", ",", "\342\200\216\342\210\222", 1, 3},
    {",", "\302\240", "\342\210\222", 1, 3},
    {",", "\342\200\257", "-", 1, 3},
    {",", ".", "\342\210\222", 2, 3},
};

/* Unknown languages take this one, as they take English plurals. */
#define CI18N_NUMBER_DEFAULT_SHAPE 5

static const ci18n_number_language_t ci18n_number_languages[] = {
    {"af", 0}, {"ar", 1}, {"az", 2}, {"be", 3}, {"bg", 3}, {"bn", 4},
    {"bs", 2}, {"ca", 2}, {"cs", 0}, {"da", 2}, {"de", 2}, {"el", 2},
    {"en", 5}, {"es", 6}, {"et", 7}, {"eu", 8}, {"fa", 9}, {"fi", 10},
    {"fr", 11}, {"ga", 5}, {"gu", 4}, {"he", 1}, {"hi", 4}, {"hr", 8},
    {"hu", 3}, {"hy", 3}, {"id", 2}, {"ig", 5}, {"it", 6}, {"iw", 1},
    {"ja", 5}, {"ka", 3}, {"kk", 0}, {"km", 5}, {"kn", 5}, {"ko", 5},
    {"ky", 0}, {"lo", 2}, {"lt", 10}, {"lv", 3}, {"ml", 4}, {"mn", 5},
    {"mr", 4}, {"ms", 5}, {"my", 5}, {"nb", 10}, {"ne", 4}, {"nl", 2},
    {"nn", 10}, {"no", 10}, {"pl", 3}, {"pt", 2}, {"ro", 2}, {"ru", 0},
    {"si", 5}, {"sk", 0}, {"sl", 12}, {"sq", 3}, {"sr", 2}, {"sv", 10},
    {"sw", 5}, {"ta", 4}, {"te", 4}, {"th", 5}, {"tr", 2}, {"uk", 0},
    {"ur", 1}, {"uz", 0}, {"vi", 2}, {"yo", 5}, {"zh", 5}, {"zu", 5},
};
/* END generated by tools/cldr_numbers.py */

static const ci18n_number_shape_t *ci18n_number_shape(const char *language_code)
{
    size_t len;
    size_t i;

    if (language_code)
    {
        len = ci18n_primary_subtag_len(language_code);
        for (i = 0; i < sizeof(ci18n_number_languages) / sizeof(ci18n_number_languages[0]); i++)
        {
            if (ci18n_subtag_eq(ci18n_number_languages[i].code, language_code, len))
            {
                return &ci18n_number_shapes[ci18n_number_languages[i].shape];
            }
        }
    }
    return &ci18n_number_shapes[CI18N_NUMBER_DEFAULT_SHAPE];
}

/* Longest number, in digits, the formatter takes apart; anything longer is
 * passed through as it came. */
#define CI18N_NUMBER_MAX_DIGITS 64

CI18N_DEF size_t ci18n_format_number(char *out, size_t capacity,
                                     const char *language_code,
                                     const char *number, int fraction_digits)
{
    const ci18n_number_shape_t *shape = ci18n_number_shape(language_code);
    char digits[CI18N_NUMBER_MAX_DIGITS + 24];
    const char *p = number;
    const char *int_start;
    const char *frac_start = NULL;
    size_t int_len;
    size_t frac_len = 0;
    size_t n_int;
    size_t n_frac;
    size_t i;
    bool negative = false;
    bool nonzero = false;
    ci18n_sink_t sink;

    sink.out = out;
    sink.capacity = out ? capacity : 0;
    sink.written = 0;
    sink.needed = 0;
    sink.full = false;

    if (out && capacity > 0)
    {
        out[0] = '\0';
    }
    if (!number)
    {
        return 0;
    }

    /* [+-]digits[.digits], nothing else. */
    if (*p == '-' || *p == '+')
    {
        negative = (*p == '-');
        p++;
    }
    int_start = p;
    while (*p >= '0' && *p <= '9')
    {
        p++;
    }
    int_len = (size_t)(p - int_start);
    if (*p == '.')
    {
        frac_start = ++p;
        while (*p >= '0' && *p <= '9')
        {
            p++;
        }
        frac_len = (size_t)(p - frac_start);
    }

    if (*p != '\0' || int_len + frac_len == 0 || fraction_digits > 20 ||
        int_len + frac_len > CI18N_NUMBER_MAX_DIGITS)
    {
        /* Not a number this can take apart: show it as it came, rather than
         * lose it. */
        ci18n_sink_put(&sink, number, strlen(number));
        if (sink.capacity > 0)
        {
            out[sink.written] = '\0';
        }
        return sink.needed;
    }

    /* Leading zeros go, one stays: "007" is 7, ".5" is 0.5. */
    while (int_len > 1 && *int_start == '0')
    {
        int_start++;
        int_len--;
    }

    /* One slot spare at the front for a carry out of the top digit. */
    digits[0] = '0';
    memcpy(digits + 1, int_start, int_len);
    n_int = int_len;

    if (fraction_digits < 0)
    {
        if (frac_len > 0)
        {
            memcpy(digits + 1 + n_int, frac_start, frac_len);
        }
        n_frac = frac_len;
    }
    else
    {
        size_t keep = (size_t)fraction_digits;
        bool up = false;

        for (i = 0; i < keep; i++)
        {
            digits[1 + n_int + i] = (i < frac_len) ? frac_start[i] : '0';
        }
        n_frac = keep;

        /* Round half to even, as CLDR and ICU do by default. */
        if (keep < frac_len)
        {
            char first = frac_start[keep];
            bool rest = false;

            for (i = keep + 1; i < frac_len; i++)
            {
                rest = rest || frac_start[i] != '0';
            }
            if (first > '5' || (first == '5' && rest))
            {
                up = true;
            }
            else if (first == '5')
            {
                up = (n_int + n_frac > 0) && ((digits[n_int + n_frac] - '0') % 2 == 1);
            }
        }

        if (up)
        {
            i = n_int + n_frac;
            while (digits[i] == '9')
            {
                digits[i--] = '0';
            }
            digits[i]++;
            if (i == 0)
            {
                n_int++; /* the carry reached the spare slot */
            }
        }
    }

    {
        /* Where the digits start: the spare slot only if a carry used it. */
        const char *d = (n_int > int_len) ? digits : digits + 1;
        size_t lead;

        for (i = 0; i < n_int + n_frac; i++)
        {
            nonzero = nonzero || d[i] != '0';
        }
        if (negative && nonzero)
        {
            ci18n_sink_put(&sink, shape->minus, strlen(shape->minus));
        }

        if (n_int == 0)
        {
            /* ".5" is 0.5: a zero before the separator. */
            ci18n_sink_put(&sink, "0", 1);
        }
        else if (n_int < (size_t)3 + shape->min_grouping)
        {
            ci18n_sink_put(&sink, d, n_int);
        }
        else
        {
            size_t sec = shape->secondary;
            size_t pos;

            lead = n_int - 3;
            pos = (lead % sec) ? lead % sec : sec;
            ci18n_sink_put(&sink, d, pos);
            while (pos < lead)
            {
                ci18n_sink_put(&sink, shape->group, strlen(shape->group));
                ci18n_sink_put(&sink, d + pos, sec);
                pos += sec;
            }
            ci18n_sink_put(&sink, shape->group, strlen(shape->group));
            ci18n_sink_put(&sink, d + lead, 3);
        }

        if (n_frac > 0)
        {
            ci18n_sink_put(&sink, shape->decimal, strlen(shape->decimal));
            ci18n_sink_put(&sink, d + n_int, n_frac);
        }
    }

    if (sink.capacity > 0)
    {
        out[sink.written] = '\0';
    }
    return sink.needed;
}
#endif

#if !defined(CI18N_NO_FORMAT) && !defined(CI18N_NO_NUMBERS)
static size_t ci18n_number_formatter(char *out, size_t capacity, const char *value,
                                     const char *arg, void *user_data)
{
    int digits = -1;

    /* "{n:number,2}": the argument is the number of fraction digits. */
    if (arg[0] >= '0' && arg[0] <= '9' && (arg[1] == '\0' ||
                                          (arg[1] >= '0' && arg[1] <= '9' && arg[2] == '\0')))
    {
        digits = (arg[1] == '\0') ? arg[0] - '0' : (arg[0] - '0') * 10 + (arg[1] - '0');
    }

    return ci18n_format_number(out, capacity, (const char *)user_data, value, digits);
}
#endif

/* Build "key[suffix]", or report that it will not fit. */
static bool ci18n_plural_key(char *out, size_t capacity, const char *key, const char *suffix)
{
    size_t key_len = strlen(key);
    size_t suffix_len = strlen(suffix);

    if (key_len + suffix_len + 3 > capacity)
    {
        return false;
    }

    memcpy(out, key, key_len);
    out[key_len] = '[';
    memcpy(out + key_len + 1, suffix, suffix_len);
    out[key_len + 1 + suffix_len] = ']';
    out[key_len + 2 + suffix_len] = '\0';
    return true;
}

/*
 * Find the right form of `key` for `count`: key[category], then key[other],
 * then key itself. Cardinals and ordinals share this and differ only in how
 * the category is chosen.
 */
/* One language's entry for a key, or NULL. Sets no error: the callers try
 * several before deciding the key is missing. */
static const char *ci18n_lookup_one(ci18n_t *ctx, const char *code, const char *key)
{
    ci18n_language_t *lang;
    int lang_idx;
    int entry_idx;

    lang_idx = ci18n_find_language(ctx, code);
    if (lang_idx < 0)
    {
        return NULL;
    }

    lang = &ctx->languages[lang_idx];
    entry_idx = ci18n_find_entry(lang, key);
    return (entry_idx >= 0)
               ? ci18n_arena_at(&lang->strings, lang->entries[entry_idx].value)
               : NULL;
}

/* The form of `key` for `count` in one language, with that language's own
 * rules picking the form. */
static const char *ci18n_forms_one(ci18n_t *ctx, const char *code, const char *key,
                                   long count, bool ordinal)
{
    char buffer[CI18N_MAX_KEY_LENGTH];
    ci18n_plural_category_t category;
    const char *result;

    if (code[0] == '\0')
    {
        return NULL;
    }

#ifndef CI18N_NO_ORDINALS
    category = ordinal ? ci18n_ordinal_category(code, count)
                       : ci18n_plural_category(code, count);
#else
    (void)ordinal;
    category = ci18n_plural_category(code, count);
#endif

    /* The exact form for this count. */
    if (ci18n_plural_key(buffer, sizeof(buffer), key, ci18n_plural_category_name(category)))
    {
        result = ci18n_lookup_one(ctx, code, buffer);
        if (result)
        {
            return result;
        }
    }

    /* The catch-all form, for a translation that only bothered with two. */
    if (category != CI18N_PLURAL_OTHER &&
        ci18n_plural_key(buffer, sizeof(buffer), key, "other"))
    {
        result = ci18n_lookup_one(ctx, code, buffer);
        if (result)
        {
            return result;
        }
    }

    /* A translation with no plural forms at all. */
    return ci18n_lookup_one(ctx, code, key);
}

static const char *ci18n_forms_impl(ci18n_t *ctx, const char *key, long count,
                                    bool ordinal)
{
    const char *result;

    if (!ctx->initialized)
    {
        ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
        return NULL;
    }

    if (!key)
    {
        ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
        return NULL;
    }

    /* One language at a time, each under its own rules. Asking the current
     * language which form to use and then taking that form from the
     * fallback gave English text Arabic grammar: "3th" where Arabic, with
     * a single ordinal form, picks "other". */
    result = ci18n_forms_one(ctx, ctx->current_language, key, count, ordinal);
    if (!result)
    {
        result = ci18n_forms_one(ctx, ctx->fallback_language, key, count, ordinal);
    }

    if (result)
    {
        ci18n_succeed(ctx);
        return result;
    }

    ci18n_fail(ctx, CI18N_ERR_KEY_NOT_FOUND);
    return NULL;
}
CI18N_DEF const char *ci18n_plural(const char *key, long count)
{
    const char *result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_plural_impl(&ci18n_ctx, key, count);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


static const char *ci18n_plural_impl(ci18n_t *ctx, const char *key, long count)
{
    return ci18n_forms_impl(ctx, key, count, false);
}

#if !defined(CI18N_NO_ORDINALS)
static const char *ci18n_ordinal_impl(ci18n_t *ctx, const char *key, long count)
{
    return ci18n_forms_impl(ctx, key, count, true);
}
#endif

static const char *ci18n_plural_or_key_impl(ci18n_t *ctx, const char *key, long count)
{
    const char *result = ci18n_plural_impl(ctx, key, count);

    return result ? result : key;
}

#if !defined(CI18N_NO_ORDINALS)
static const char *ci18n_ordinal_or_key_impl(ci18n_t *ctx, const char *key, long count)
{
    const char *result = ci18n_ordinal_impl(ctx, key, count);

    return result ? result : key;
}
#endif
CI18N_DEF const char *ci18n_plural_or_key(const char *key, long count)
{
    const char *result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_plural_or_key_impl(&ci18n_ctx, key, count);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


/* ============================================================================
 * Locale detection
 * ============================================================================ */

#if !defined(CI18N_NO_LOCALE)
/*
 * Copy a locale name, dropping everything the language tag does not need.
 *
 * "ru_RU.UTF-8@euro" becomes "ru-RU": the encoding and the modifier say
 * nothing about which translation to pick, and underscores are spelled as
 * hyphens so one form reaches the caller.
 */
static size_t ci18n_normalize_locale(char *out, size_t capacity, const char *locale)
{
    size_t n = 0;
    size_t i;

    if (capacity == 0)
    {
        return 0;
    }

    for (i = 0; locale[i] != '\0'; i++)
    {
        char c = locale[i];

        if (c == '.' || c == '@')
        {
            break;
        }

        if (n + 1 >= capacity)
        {
            break;
        }

        out[n++] = (c == '_') ? '-' : c;
    }

    out[n] = '\0';

    /* "C" and "POSIX" are the absence of a locale, not a language. */
    if (strcmp(out, "C") == 0 || strcmp(out, "POSIX") == 0)
    {
        out[0] = '\0';
        return 0;
    }

    return n;
}

CI18N_DEF size_t ci18n_detect_locale(char *out, size_t capacity)
{
    ci18n_t *ctx = &ci18n_ctx;

    static const char *variables[] = {"LC_ALL", "LC_MESSAGES", "LANG"};
    size_t i;

    if (!out || capacity == 0)
    {
        ci18n_fail(ctx, CI18N_ERR_INVALID_ARGUMENT);
        return 0;
    }

    out[0] = '\0';

    /* Environment first, on every platform: it is what a user overriding the
     * language for one program will have set. */
    for (i = 0; i < sizeof(variables) / sizeof(variables[0]); i++)
    {
        const char *value;

        CI18N_CRT_WARNINGS_OFF
        value = getenv(variables[i]);
        CI18N_CRT_WARNINGS_ON

        if (value && value[0] != '\0')
        {
            size_t len = ci18n_normalize_locale(out, capacity, value);

            if (len > 0)
            {
                ci18n_succeed(ctx);
                return len;
            }
        }
    }

#if defined(_WIN32) && !defined(CI18N_NO_PLATFORM_LOCALE)
    /* Those variables are normally unset on Windows, so ask the system. */
    {
        wchar_t wide[LOCALE_NAME_MAX_LENGTH];
        int count = GetUserDefaultLocaleName(wide, LOCALE_NAME_MAX_LENGTH);

        if (count > 0)
        {
            char narrow[LOCALE_NAME_MAX_LENGTH];
            int j;

            /* Locale names are ASCII, so a byte-wise narrowing is enough and
             * avoids dragging in a conversion function. */
            for (j = 0; j < count && j < (int)sizeof(narrow) - 1; j++)
            {
                narrow[j] = (wide[j] < 128) ? (char)wide[j] : '?';
            }
            narrow[j] = '\0';

            if (narrow[0] != '\0')
            {
                size_t len = ci18n_normalize_locale(out, capacity, narrow);

                if (len > 0)
                {
                    ci18n_succeed(ctx);
                    return len;
                }
            }
        }
    }
#endif

    ci18n_succeed(ctx);
    return 0;
}

static bool ci18n_set_current_best_impl(ci18n_t *ctx, const char *locale)
{
    char candidate[CI18N_MAX_CODE_LENGTH];
    char detected[CI18N_MAX_CODE_LENGTH];
    size_t len;

    if (!ctx->initialized)
    {
        return ci18n_fail(ctx, CI18N_ERR_NOT_INITIALIZED);
    }

    if (!locale)
    {
        if (ci18n_detect_locale(detected, sizeof(detected)) == 0)
        {
            return ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
        }

        locale = detected;
    }

    if (!ci18n_code_fits(locale))
    {
        return ci18n_fail(ctx, CI18N_ERR_CODE_TOO_LONG);
    }

    ci18n_copy(candidate, sizeof(candidate), locale);
    len = strlen(candidate);

    /* Walk from the most specific form to the least: ru-RU, then ru. Both
     * separators are accepted, since a caller may pass either spelling. */
    for (;;)
    {
        if (len > 0 && ci18n_find_language(ctx, candidate) >= 0)
        {
            ci18n_copy(ctx->current_language, sizeof(ctx->current_language),
                       candidate);
            ci18n_succeed(ctx);
            return true;
        }

        while (len > 0 && candidate[len - 1] != '-' && candidate[len - 1] != '_')
        {
            len--;
        }

        if (len == 0)
        {
            break;
        }

        /* Drop the separator too, then try the shorter tag. */
        len--;
        candidate[len] = '\0';
    }

    return ci18n_fail(ctx, CI18N_ERR_LANGUAGE_NOT_FOUND);
}
CI18N_DEF bool ci18n_set_current_best(const char *locale)
{
    bool result;

    CI18N_WRITE_LOCK(&ci18n_ctx);
    result = ci18n_set_current_best_impl(&ci18n_ctx, locale);
    CI18N_WRITE_UNLOCK(&ci18n_ctx);

    return result;
}
#endif


static bool ci18n_is_initialized_impl(ci18n_t *ctx)
{
    return ctx->initialized;
}
/* ============================================================================
 * Catalogues
 * ============================================================================ */

CI18N_DEF ci18n_t *ci18n_create(void)
{
    ci18n_t *catalog = (ci18n_t *)calloc(1, sizeof(ci18n_t));

    if (!catalog)
    {
        return NULL;
    }

#ifdef CI18N_THREAD_SHARED
    /* The default catalogue gets a static initialiser; one made at runtime
     * has to be initialised here. calloc leaves it zeroed, which happens to
     * be right for SRWLOCK and is not something to rely on for pthreads. */
    {
        unsigned i;
        for (i = 0; i < CI18N_LOCK_SHARDS; i++)
        {
#if defined(_WIN32)
            InitializeSRWLock(&catalog->locks[i].lock);
#else
            if (pthread_rwlock_init(&catalog->locks[i].lock, NULL) != 0)
            {
                while (i-- > 0)
                {
                    pthread_rwlock_destroy(&catalog->locks[i].lock);
                }
                free(catalog);
                return NULL;
            }
#endif
        }
    }
#endif

    catalog->initialized = true;
    return catalog;
}

CI18N_DEF void ci18n_destroy(ci18n_t *catalog)
{
    size_t i;

    if (!catalog)
    {
        return;
    }

    for (i = 0; i < catalog->language_count; i++)
    {
        ci18n_lang_release(&catalog->languages[i]);
    }

#ifdef CI18N_THREAD_SHARED
#if !defined(_WIN32)
    for (i = 0; i < CI18N_LOCK_SHARDS; i++)
    {
        pthread_rwlock_destroy(&catalog->locks[i].lock);
    }
#endif
#endif

    free(catalog);
}

CI18N_DEF ci18n_t *ci18n_default(void)
{
    return &ci18n_ctx;
}

#if !defined(CI18N_NO_FILES)
CI18N_DEF bool ci18n_load_language_in(ci18n_t *catalog, const char *language_code, const char *filepath)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_load_language_impl(catalog, language_code, filepath);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}
#endif

CI18N_DEF bool ci18n_load_from_buffer_in(ci18n_t *catalog, const char *language_code, const char *buffer, size_t length)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_load_from_buffer_impl(catalog, language_code, buffer, length);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_set_current_in(ci18n_t *catalog, const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_set_current_impl(catalog, language_code);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_set_fallback_in(ci18n_t *catalog, const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_set_fallback_impl(catalog, language_code);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

#if !defined(CI18N_NO_LOCALE)
CI18N_DEF bool ci18n_set_current_best_in(ci18n_t *catalog, const char *locale)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_set_current_best_impl(catalog, locale);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}
#endif

CI18N_DEF bool ci18n_set_in(ci18n_t *catalog, const char *language_code, const char *key, const char *value)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_set_impl(catalog, language_code, key, value);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_remove_in(ci18n_t *catalog, const char *language_code, const char *key)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_remove_impl(catalog, language_code, key);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_clear_in(ci18n_t *catalog, const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_clear_impl(catalog, language_code);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_remove_language_in(ci18n_t *catalog, const char *language_code)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_remove_language_impl(catalog, language_code);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

CI18N_DEF const char *ci18n_get_in(ci18n_t *catalog, const char *key)
{
    const char *result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_get_impl(catalog, key);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF const char *ci18n_get_or_key_in(ci18n_t *catalog, const char *key)
{
    const char *result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_get_or_key_impl(catalog, key);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF bool ci18n_has_in(ci18n_t *catalog, const char *key)
{
    bool result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_has_impl(catalog, key);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF const char *ci18n_get_current_in(ci18n_t *catalog)
{
    const char *result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_get_current_impl(catalog);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF size_t ci18n_get_languages_in(ci18n_t *catalog, const char **out, size_t capacity)
{
    size_t result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_get_languages_impl(catalog, out, capacity);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF size_t ci18n_count_in(ci18n_t *catalog, const char *language_code)
{
    size_t result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_count_impl(catalog, language_code);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF size_t ci18n_foreach_in(ci18n_t *catalog, const char *language_code,
                                  ci18n_entry_fn fn, void *user_data)
{
    size_t result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_foreach_impl(catalog, language_code, fn, user_data);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF const char *ci18n_plural_in(ci18n_t *catalog, const char *key, long count)
{
    const char *result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_plural_impl(catalog, key, count);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF const char *ci18n_plural_or_key_in(ci18n_t *catalog, const char *key, long count)
{
    const char *result;

    CI18N_READ_LOCK(catalog);
    result = ci18n_plural_or_key_impl(catalog, key, count);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

#if !defined(CI18N_NO_ORDINALS)
CI18N_DEF const char *ci18n_ordinal_in(ci18n_t *catalog, const char *key, long count)
{
    const char *result;

    if (!catalog)
    {
        return NULL;
    }

    CI18N_READ_LOCK(catalog);
    result = ci18n_ordinal_impl(catalog, key, count);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF const char *ci18n_ordinal_or_key_in(ci18n_t *catalog, const char *key, long count)
{
    const char *result;

    if (!catalog)
    {
        return key;
    }

    CI18N_READ_LOCK(catalog);
    result = ci18n_ordinal_or_key_impl(catalog, key, count);
    CI18N_READ_UNLOCK(catalog);

    return result;
}

CI18N_DEF const char *ci18n_ordinal(const char *key, long count)
{
    return ci18n_ordinal_in(&ci18n_ctx, key, count);
}

CI18N_DEF const char *ci18n_ordinal_or_key(const char *key, long count)
{
    return ci18n_ordinal_or_key_in(&ci18n_ctx, key, count);
}
#endif

CI18N_DEF ci18n_error_t ci18n_last_error_in(ci18n_t *catalog)
{
    return catalog->last_error;
}

CI18N_DEF const ci18n_load_stats_t *ci18n_last_load_stats_in(ci18n_t *catalog)
{
    return &catalog->load_stats;
}

CI18N_DEF size_t ci18n_get_copy_in(ci18n_t *catalog, const char *key,
                                   char *out, size_t capacity)
{
    const char *text;
    size_t len;

    if (out && capacity > 0)
    {
        out[0] = 0;
    }

    CI18N_READ_LOCK(catalog);

    text = ci18n_get_impl(catalog, key);
    if (!text)
    {
        CI18N_READ_UNLOCK(catalog);
        return 0;
    }

    len = strlen(text);

    if (out && capacity > 0)
    {
        size_t fits = len < capacity - 1 ? len : capacity - 1;

        memcpy(out, text, fits);
        out[fits] = 0;
    }

    CI18N_READ_UNLOCK(catalog);
    return len;
}

CI18N_DEF bool ci18n_is_initialized(void)
{
    bool result;

    CI18N_READ_LOCK(&ci18n_ctx);
    result = ci18n_is_initialized_impl(&ci18n_ctx);
    CI18N_READ_UNLOCK(&ci18n_ctx);

    return result;
}


CI18N_DEF ci18n_error_t ci18n_last_error(void)
{
    /* The default catalogue is named inline rather than through a local: in
     * the shared mode the slot is thread-local and the macro discards its
     * argument, which would leave the local unused. */
    return CI18N_ERROR_SLOT(&ci18n_ctx);
}

CI18N_DEF const char *ci18n_error_string(ci18n_error_t error)
{
    switch (error)
    {
    case CI18N_OK:
        return "no error";
    case CI18N_ERR_NOT_INITIALIZED:
        return "ci18n_init() has not been called";
    case CI18N_ERR_INVALID_ARGUMENT:
        return "invalid argument";
    case CI18N_ERR_CODE_TOO_LONG:
        return "language code is too long";
    case CI18N_ERR_FILE_NOT_FOUND:
        return "translation file could not be opened";
    case CI18N_ERR_OUT_OF_MEMORY:
        return "out of memory";
    case CI18N_ERR_TOO_MANY_LANGUAGES:
        return "too many languages";
    case CI18N_ERR_TOO_MANY_KEYS:
        return "too many keys in this language";
    case CI18N_ERR_LANGUAGE_NOT_FOUND:
        return "no such language is loaded";
    case CI18N_ERR_KEY_NOT_FOUND:
        return "no such key";
    case CI18N_ERR_PARSE:
        return "the load dropped or truncated something";
    case CI18N_ERR_TOO_MANY_FORMATTERS:
        return "too many formatters";
    case CI18N_ERR_UNKNOWN_FORMATTER:
        return "a translation asked for a formatter that is not registered";
    }

    return "unknown error";
}

CI18N_DEF const ci18n_load_stats_t *ci18n_last_load_stats(void)
{
    return &CI18N_STATS_SLOT(&ci18n_ctx);
}

#ifdef CI18N_THREAD_LOCAL_CONTEXT
CI18N_DEF ci18n_context_t *ci18n_get_context(void)
{
    return &ci18n_ctx;
}
#endif

#endif /* CI18N_IMPLEMENTATION */
