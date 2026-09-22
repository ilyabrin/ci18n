/*
 * ci18n.h - v2.6.0
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
 *   - Locale detection with a fallback chain: ru-RU to ru
 *   - Named interpolation, so translations decide where values go
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
#define CI18N_VERSION_MINOR 6
#define CI18N_VERSION_PATCH 0
#define CI18N_VERSION_STRING "2.6.0"

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

#if defined(_WIN32)
#include <windows.h>
typedef SRWLOCK ci18n_rwlock_t;
#define CI18N_RWLOCK_INIT SRWLOCK_INIT
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
#define ci18n_rwlock_read(l) pthread_rwlock_rdlock(l)
#define ci18n_rwlock_read_unlock(l) pthread_rwlock_unlock(l)
#define ci18n_rwlock_write(l) pthread_rwlock_wrlock(l)
#define ci18n_rwlock_write_unlock(l) pthread_rwlock_unlock(l)
#endif

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
        CI18N_ERR_PARSE              /* the load dropped or truncated something */
    } ci18n_error_t;

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

    typedef struct ci18n_context
    {
        ci18n_language_t languages[CI18N_MAX_LANGUAGES];
        size_t language_count;
        char current_language[CI18N_MAX_CODE_LENGTH];
        char fallback_language[CI18N_MAX_CODE_LENGTH];
        ci18n_error_t last_error;
        ci18n_load_stats_t load_stats;
        bool initialized;
#ifdef CI18N_THREAD_SHARED
        /* Inside the catalogue, not beside the global, so that every
         * catalogue locks itself. */
        ci18n_rwlock_t lock;
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
     * Portuguese where zero is also "one", Russian, Ukrainian and Belarusian,
     * Polish, Czech and Slovak, Croatian and Serbian, Arabic, Lithuanian,
     * Latvian, Slovenian, Irish, Romanian, and the languages with no plural
     * distinction at all such as Japanese, Chinese and Korean.
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

    /* ============================================================================
     * Locale detection
     * ============================================================================ */

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

    /* The _in variants. Each behaves exactly as the function it is named
     * after, on the catalogue given rather than on the default one. */
    CI18N_DEF bool ci18n_load_language_in(ci18n_t *catalog, const char *language_code, const char *filepath);
    CI18N_DEF bool ci18n_load_from_buffer_in(ci18n_t *catalog, const char *language_code, const char *buffer, size_t length);
    CI18N_DEF bool ci18n_set_current_in(ci18n_t *catalog, const char *language_code);
    CI18N_DEF bool ci18n_set_fallback_in(ci18n_t *catalog, const char *language_code);
    CI18N_DEF bool ci18n_set_current_best_in(ci18n_t *catalog, const char *locale);
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
    CI18N_DEF const char *ci18n_plural_in(ci18n_t *catalog, const char *key, long count);
    CI18N_DEF const char *ci18n_plural_or_key_in(ci18n_t *catalog, const char *key, long count);

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
 * ============================================================================ */

#ifdef CI18N_IMPLEMENTATION

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

#define CI18N_READ_LOCK(c) ci18n_rwlock_read(&(c)->lock)
#define CI18N_READ_UNLOCK(c) ci18n_rwlock_read_unlock(&(c)->lock)
#define CI18N_WRITE_LOCK(c) ci18n_rwlock_write(&(c)->lock)
#define CI18N_WRITE_UNLOCK(c) ci18n_rwlock_write_unlock(&(c)->lock)

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

static ci18n_context_t ci18n_ctx;

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


static bool ci18n_load_language_impl(ci18n_t *ctx, const char *language_code, const char *filepath)
{
    FILE *file;
    char line[CI18N_MAX_LINE_LENGTH];
    ci18n_language_t *lang;
    size_t line_number = 0;

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

    file = fopen(filepath, "r");
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

    ci18n_reset_load_stats(ctx);

    while (fgets(line, sizeof(line), file))
    {
        size_t len = strlen(line);

        line_number++;

        /* fgets() stops at the buffer, so a line longer than it would come
         * back on the next read and be parsed as a line of its own, usually
         * without a separator, which then looked like a malformed line that
         * was never in the file. Swallow the tail instead. */
        if (len == sizeof(line) - 1 && line[len - 1] != '\n' && line[len - 1] != '\r')
        {
            int discarded;

            CI18N_STATS_SLOT(ctx).lines_truncated++;

            while ((discarded = fgetc(file)) != EOF && discarded != '\n')
            {
                /* nothing: the rest of this line cannot be stored anyway */
            }
        }

        /* Strip the line terminator. One loop covers LF, CRLF and a lone CR,
         * so a file authored on any platform parses the same way. */
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }

        ci18n_record_line(ctx, ci18n_parse_line(ctx, lang, line), line_number);
    }

    fclose(file);
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


static bool ci18n_load_from_buffer_impl(ci18n_t *ctx, const char *language_code, const char *buffer, size_t length)
{
    ci18n_language_t *lang;
    char line[CI18N_MAX_LINE_LENGTH];
    size_t pos = 0;
    size_t line_pos = 0;
    size_t line_number = 0;
    bool line_cut = false;

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

    ci18n_reset_load_stats(ctx);

    while (pos < length)
    {
        char c = buffer[pos++];

        if (c == '\n' || c == '\r')
        {
            line[line_pos] = '\0';
            line_number++;
            ci18n_record_line(ctx, ci18n_parse_line(ctx, lang, line), line_number);
            line_pos = 0;
            line_cut = false;

            /* Skip \r\n pairs */
            if (c == '\r' && pos < length && buffer[pos] == '\n')
            {
                pos++;
            }
        }
        else
        {
            if (line_pos < CI18N_MAX_LINE_LENGTH - 1)
            {
                line[line_pos++] = c;
            }
            else if (!line_cut)
            {
                /* Everything past the buffer is dropped, unlike the file
                 * loader where the tail resurfaces as another line. */
                CI18N_STATS_SLOT(ctx).lines_truncated++;
                line_cut = true;
            }
        }
    }

    /* Process last line if no newline at end */
    if (line_pos > 0)
    {
        line[line_pos] = '\0';
        line_number++;
        ci18n_record_line(ctx, ci18n_parse_line(ctx, lang, line), line_number);
    }

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

/*
 * Writes into the caller's buffer while counting what the whole result would
 * need, so one pass can both fill a buffer and report a too-small one.
 */
typedef struct ci18n_sink
{
    char *out;
    size_t capacity;
    size_t written; /* bytes actually placed, never past capacity - 1 */
    size_t needed;  /* bytes the full result would take */
} ci18n_sink_t;

static void ci18n_sink_put(ci18n_sink_t *sink, const char *text, size_t len)
{
    sink->needed += len;

    if (sink->capacity == 0)
    {
        return;
    }

    if (sink->written + len > sink->capacity - 1)
    {
        len = sink->capacity - 1 - sink->written;
    }

    if (len > 0)
    {
        memcpy(sink->out + sink->written, text, len);
        sink->written += len;
    }
}

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
 * Expand {placeholders} in `text`.
 *
 * `count_text` is the pre-rendered number for {count}, or NULL when there is
 * no count. An explicit pair of the same name still wins, so a caller can
 * override it.
 */
static size_t ci18n_expand(char *out, size_t capacity, const char *text,
                           va_list args, const char *count_text)
{
    ci18n_sink_t sink;
    size_t i = 0;

    sink.out = out;
    sink.capacity = capacity;
    sink.written = 0;
    sink.needed = 0;

    while (text[i] != '\0')
    {
        size_t start;
        size_t name_len;
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
        name_len = 0;
        while (text[start + name_len] != '\0' && text[start + name_len] != '}')
        {
            name_len++;
        }

        /* Unterminated: the rest of the string is literal, not a placeholder. */
        if (text[start + name_len] != '}')
        {
            ci18n_sink_put(&sink, text + i, strlen(text + i));
            break;
        }

        value = ci18n_lookup_argument(args, text + start, name_len);

        if (!value && count_text &&
            name_len == 5 && strncmp(text + start, "count", 5) == 0)
        {
            value = count_text;
        }

        if (value)
        {
            ci18n_sink_put(&sink, value, strlen(value));
        }
        else
        {
            /* No such name: leave the placeholder visible, so a typo in a
             * translation is something you can see rather than a hole. */
            ci18n_sink_put(&sink, text + i, name_len + 2);
        }

        i = start + name_len + 1;
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

CI18N_DEF size_t ci18n_format(char *out, size_t capacity, const char *key, ...)
{
    ci18n_t *ctx = &ci18n_ctx;

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
    needed = ci18n_expand(out, out ? capacity : 0, text, args, NULL);
    va_end(args);

    ci18n_succeed(ctx);
    CI18N_READ_UNLOCK(&ci18n_ctx);
    return needed;
}

CI18N_DEF size_t ci18n_format_plural(char *out, size_t capacity, const char *key,
                                     long count, ...)
{
    ci18n_t *ctx = &ci18n_ctx;

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
    needed = ci18n_expand(out, out ? capacity : 0, text, args, count_text);
    va_end(args);

    ci18n_succeed(ctx);
    CI18N_READ_UNLOCK(&ci18n_ctx);
    return needed;
}


/*
 * CLDR groups languages by the plural rule they follow, so the rules live
 * here as families and the table below maps languages onto them. Writing out
 * one rule per language would be a few hundred near-duplicates.
 */
typedef enum ci18n_plural_family
{
    CI18N_PF_OTHER_ONLY,  /* ja, zh, ko: no plural distinction at all */
    CI18N_PF_ONE_OTHER,   /* en, de, es: one for exactly 1 */
    CI18N_PF_ZERO_ONE,    /* fr, pt, hi: 0 counts as one too */
    CI18N_PF_SLAVIC,      /* ru, uk, be */
    CI18N_PF_POLISH,      /* pl */
    CI18N_PF_CZECH,       /* cs, sk */
    CI18N_PF_BALKAN,      /* hr, sr, bs */
    CI18N_PF_ARABIC,      /* ar */
    CI18N_PF_LITHUANIAN,  /* lt */
    CI18N_PF_LATVIAN,     /* lv */
    CI18N_PF_SLOVENIAN,   /* sl */
    CI18N_PF_IRISH,       /* ga */
    CI18N_PF_ROMANIAN     /* ro */
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
    {"fr", CI18N_PF_ZERO_ONE}, {"pt", CI18N_PF_ZERO_ONE},
    {"hi", CI18N_PF_ZERO_ONE}, {"bn", CI18N_PF_ZERO_ONE},
    {"fa", CI18N_PF_ZERO_ONE}, {"hy", CI18N_PF_ZERO_ONE},
    {"gu", CI18N_PF_ZERO_ONE}, {"kn", CI18N_PF_ZERO_ONE},
    {"zu", CI18N_PF_ZERO_ONE}, {"mr", CI18N_PF_ZERO_ONE},

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

    /* One for exactly 1. The default, so these are here for documentation as
     * much as for lookup. */
    {"en", CI18N_PF_ONE_OTHER}, {"de", CI18N_PF_ONE_OTHER},
    {"nl", CI18N_PF_ONE_OTHER}, {"sv", CI18N_PF_ONE_OTHER},
    {"da", CI18N_PF_ONE_OTHER}, {"no", CI18N_PF_ONE_OTHER},
    {"nb", CI18N_PF_ONE_OTHER}, {"nn", CI18N_PF_ONE_OTHER},
    {"fi", CI18N_PF_ONE_OTHER}, {"et", CI18N_PF_ONE_OTHER},
    {"el", CI18N_PF_ONE_OTHER}, {"es", CI18N_PF_ONE_OTHER},
    {"it", CI18N_PF_ONE_OTHER}, {"hu", CI18N_PF_ONE_OTHER},
    {"bg", CI18N_PF_ONE_OTHER}, {"sq", CI18N_PF_ONE_OTHER},
    {"ka", CI18N_PF_ONE_OTHER}, {"eu", CI18N_PF_ONE_OTHER},
    {"tr", CI18N_PF_ONE_OTHER}, {"az", CI18N_PF_ONE_OTHER},
    {"kk", CI18N_PF_ONE_OTHER}, {"uz", CI18N_PF_ONE_OTHER},
    {"ky", CI18N_PF_ONE_OTHER}, {"mn", CI18N_PF_ONE_OTHER},
    {"ne", CI18N_PF_ONE_OTHER}, {"sw", CI18N_PF_ONE_OTHER},
    {"af", CI18N_PF_ONE_OTHER}, {"he", CI18N_PF_ONE_OTHER},
    {"ta", CI18N_PF_ONE_OTHER}, {"te", CI18N_PF_ONE_OTHER},
    {"ml", CI18N_PF_ONE_OTHER}, {"si", CI18N_PF_ONE_OTHER},
    {"ur", CI18N_PF_ONE_OTHER}, {"ca", CI18N_PF_ONE_OTHER}
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

static ci18n_plural_family_t ci18n_plural_family(const char *language_code)
{
    size_t len;
    size_t i;

    if (!language_code)
    {
        return CI18N_PF_ONE_OTHER;
    }

    len = ci18n_primary_subtag_len(language_code);

    for (i = 0; i < sizeof(ci18n_plural_rules) / sizeof(ci18n_plural_rules[0]); i++)
    {
        const char *candidate = ci18n_plural_rules[i].language;

        if (strlen(candidate) == len && strncmp(candidate, language_code, len) == 0)
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

    case CI18N_PF_ONE_OTHER:
    default:
        return (n == 1) ? CI18N_PLURAL_ONE : CI18N_PLURAL_OTHER;
    }
}

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

static const char *ci18n_plural_impl(ci18n_t *ctx, const char *key, long count)
{
    char buffer[CI18N_MAX_KEY_LENGTH];
    ci18n_plural_category_t category;
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

    category = ci18n_plural_category(ctx->current_language, count);

    /* The exact form for this count. */
    if (ci18n_plural_key(buffer, sizeof(buffer), key, ci18n_plural_category_name(category)))
    {
        result = ci18n_get_impl(ctx, buffer);
        if (result)
        {
            return result;
        }
    }

    /* The catch-all form, for a translation that only bothered with two. */
    if (category != CI18N_PLURAL_OTHER &&
        ci18n_plural_key(buffer, sizeof(buffer), key, "other"))
    {
        result = ci18n_get_impl(ctx, buffer);
        if (result)
        {
            return result;
        }
    }

    /* A translation with no plural forms at all. */
    result = ci18n_get_impl(ctx, key);
    if (result)
    {
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


static const char *ci18n_plural_or_key_impl(ci18n_t *ctx, const char *key, long count)
{
    const char *result = ci18n_plural_impl(ctx, key, count);

    return result ? result : key;
}
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
        const char *value = getenv(variables[i]);

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
#if defined(_WIN32)
    InitializeSRWLock(&catalog->lock);
#else
    if (pthread_rwlock_init(&catalog->lock, NULL) != 0)
    {
        free(catalog);
        return NULL;
    }
#endif
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
    pthread_rwlock_destroy(&catalog->lock);
#endif
#endif

    free(catalog);
}

CI18N_DEF ci18n_t *ci18n_default(void)
{
    return &ci18n_ctx;
}

CI18N_DEF bool ci18n_load_language_in(ci18n_t *catalog, const char *language_code, const char *filepath)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_load_language_impl(catalog, language_code, filepath);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

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

CI18N_DEF bool ci18n_set_current_best_in(ci18n_t *catalog, const char *locale)
{
    bool result;

    CI18N_WRITE_LOCK(catalog);
    result = ci18n_set_current_best_impl(catalog, locale);
    CI18N_WRITE_UNLOCK(catalog);

    return result;
}

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
