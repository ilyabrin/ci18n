/*
 * ci18n.h - v2.0.0
 * Single-header internationalization (i18n) library for C projects
 *
 * Features:
 *   - Simple translation key-value storage
 *   - Multiple language support
 *   - UTF-8 string handling, BOM tolerant
 *   - Thread-local context option
 *   - No external dependencies, C99 and newer
 *   - Packed storage: an entry costs 16 bytes, not 4352
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
#define CI18N_VERSION_MINOR 0
#define CI18N_VERSION_PATCH 0
#define CI18N_VERSION_STRING "2.0.0"

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
    } ci18n_context_t;

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
     * Optional: thread-local context
     * ============================================================================
     *
     * Define CI18N_THREAD_LOCAL_CONTEXT before including this header to give
     * every thread its own context.
     *
     * Read that literally: it is isolation, not shared thread safety. Each
     * thread starts empty and must call ci18n_init() and load its own
     * translations, and a language loaded on one thread is invisible to the
     * others. It suits a worker that renders in one user's locale.
     *
     * What it does NOT give you is "load once, read from N threads". Without
     * this macro the context is a single global with no locking, so concurrent
     * ci18n_set() or ci18n_load_*() against concurrent ci18n_get() is a data
     * race. If your threads only read, and every load happened before you
     * spawned them, the default global is safe as is.
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

#ifdef CI18N_THREAD_LOCAL_CONTEXT
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
#error "CI18N_THREAD_LOCAL_CONTEXT requires a compiler with thread-local storage"
#endif
static CI18N_THREAD_LOCAL ci18n_context_t ci18n_ctx;
#else
static ci18n_context_t ci18n_ctx;
#endif

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
static bool ci18n_fail(ci18n_error_t error)
{
    ci18n_ctx.last_error = error;
    return false;
}

/* Record success */
static void ci18n_succeed(void)
{
    ci18n_ctx.last_error = CI18N_OK;
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
static bool ci18n_arena_reserve(ci18n_arena_t *arena, size_t need)
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
        return ci18n_fail(CI18N_ERR_OUT_OF_MEMORY);
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
static bool ci18n_arena_add(ci18n_arena_t *arena, const char *text, size_t len,
                            uint32_t *out_offset)
{
    if (!ci18n_arena_reserve(arena, len + 1))
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
static bool ci18n_ensure_buckets(ci18n_language_t *lang, size_t wanted)
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
        return ci18n_fail(CI18N_ERR_OUT_OF_MEMORY);
    }

    lang->buckets = buckets;
    lang->bucket_count = bucket_count;
    ci18n_rebuild_buckets(lang);
    return true;
}

/* Make room for one more entry, doubling as it grows. */
static bool ci18n_ensure_capacity(ci18n_language_t *lang)
{
    size_t capacity = lang->capacity;
    ci18n_entry_t *entries;

    if (lang->count < capacity)
    {
        return true;
    }

    if (lang->count >= CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        return ci18n_fail(CI18N_ERR_TOO_MANY_KEYS);
    }

    capacity = capacity == 0 ? 8 : capacity * 2;
    if (capacity > CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        capacity = CI18N_MAX_KEYS_PER_LANGUAGE;
    }

    entries = (ci18n_entry_t *)realloc(lang->entries, sizeof(ci18n_entry_t) * capacity);
    if (!entries)
    {
        return ci18n_fail(CI18N_ERR_OUT_OF_MEMORY);
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
static bool ci18n_lang_set(ci18n_language_t *lang,
                           const char *key, size_t key_len,
                           const char *value, size_t value_len)
{
    uint32_t hash = ci18n_hash(key, key_len);
    uint32_t key_offset;
    uint32_t value_offset;
    ci18n_entry_t *entry;
    size_t bucket;
    int existing = ci18n_find_entry_n(lang, key, key_len, hash);

    if (existing >= 0)
    {
        entry = &lang->entries[existing];

        if (strlen(ci18n_arena_at(&lang->strings, entry->value)) >= value_len)
        {
            char *slot = lang->strings.data + entry->value;

            memcpy(slot, value, value_len);
            slot[value_len] = '\0';
            return true;
        }

        if (!ci18n_arena_add(&lang->strings, value, value_len, &value_offset))
        {
            return false;
        }

        /* The arena may have moved, but offsets are stable, so only this one
         * field needs updating. */
        lang->entries[existing].value = value_offset;
        return true;
    }

    if (!ci18n_ensure_capacity(lang))
    {
        return false;
    }

    if (!ci18n_ensure_buckets(lang, lang->count + 1))
    {
        return false;
    }

    if (!ci18n_arena_add(&lang->strings, key, key_len, &key_offset))
    {
        return false;
    }

    if (!ci18n_arena_add(&lang->strings, value, value_len, &value_offset))
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

/* Release everything one language holds. */
static void ci18n_lang_release(ci18n_language_t *lang)
{
    free(lang->strings.data);
    free(lang->entries);
    free(lang->buckets);
    memset(lang, 0, sizeof(*lang));
}

/* Find language by code, returns index or -1 if not found */
static int ci18n_find_language(const char *code)
{
    size_t i;
    for (i = 0; i < ci18n_ctx.language_count; i++)
    {
        if (strcmp(ci18n_ctx.languages[i].code, code) == 0)
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
static ci18n_language_t *ci18n_get_or_create_language(const char *code)
{
    int idx = ci18n_find_language(code);
    ci18n_language_t *lang;

    if (idx >= 0)
    {
        return &ci18n_ctx.languages[idx];
    }

    if (ci18n_ctx.language_count >= CI18N_MAX_LANGUAGES)
    {
        ci18n_fail(CI18N_ERR_TOO_MANY_LANGUAGES);
        return NULL;
    }

    lang = &ci18n_ctx.languages[ci18n_ctx.language_count];
    memset(lang, 0, sizeof(ci18n_language_t));
    ci18n_copy(lang->code, sizeof(lang->code), code);
    ci18n_ctx.language_count++;

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
static ci18n_line_result_t ci18n_parse_line(ci18n_language_t *lang, const char *line)
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

    /* Skip empty lines and comments */
    while (*line && ci18n_is_space(*line))
        line++;
    if (*line == '\0' || *line == '#' || *line == ';')
    {
        return CI18N_LINE_SKIPPED;
    }

    /* Find equals sign */
    eq = strchr(line, '=');
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
        ci18n_ctx.load_stats.keys_truncated++;
    }

    if (value_len > CI18N_MAX_VALUE_LENGTH - 1)
    {
        value_len = CI18N_MAX_VALUE_LENGTH - 1;
        ci18n_ctx.load_stats.values_truncated++;
    }

    if (!ci18n_lang_set(lang, key, key_len, value, value_len))
    {
        return CI18N_LINE_FAILED;
    }

    return CI18N_LINE_LOADED;
}

/* Fold one line's outcome into the stats of the load in progress */
static void ci18n_record_line(ci18n_line_result_t result, size_t line_number)
{
    ci18n_ctx.load_stats.lines_read++;

    switch (result)
    {
    case CI18N_LINE_LOADED:
        ci18n_ctx.load_stats.entries_loaded++;
        break;

    case CI18N_LINE_SKIPPED:
        ci18n_ctx.load_stats.lines_skipped++;
        break;

    case CI18N_LINE_MALFORMED:
    case CI18N_LINE_FAILED:
        ci18n_ctx.load_stats.lines_malformed++;
        if (ci18n_ctx.load_stats.first_malformed_line == 0)
        {
            ci18n_ctx.load_stats.first_malformed_line = line_number;
        }
        break;
    }
}

/* Start a load with a clean slate of statistics */
static void ci18n_reset_load_stats(void)
{
    memset(&ci18n_ctx.load_stats, 0, sizeof(ci18n_ctx.load_stats));
}

/*
 * Close out a load. Reading the source counts as success, so a file of pure
 * garbage still returns true; anything dropped or truncated is reported
 * through the error code and the stats instead.
 */
static bool ci18n_finish_load(void)
{
    const ci18n_load_stats_t *st = &ci18n_ctx.load_stats;

    if (st->lines_malformed > 0 || st->keys_truncated > 0 ||
        st->values_truncated > 0 || st->lines_truncated > 0)
    {
        ci18n_ctx.last_error = CI18N_ERR_PARSE;
    }
    else
    {
        ci18n_succeed();
    }

    return true;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

CI18N_DEF bool ci18n_init(void)
{
    if (ci18n_ctx.initialized)
    {
        return true;
    }

    memset(&ci18n_ctx, 0, sizeof(ci18n_context_t));
    ci18n_ctx.initialized = true;
    return true;
}

CI18N_DEF void ci18n_free(void)
{
    size_t i;

    if (!ci18n_ctx.initialized)
    {
        return;
    }

    for (i = 0; i < ci18n_ctx.language_count; i++)
    {
        ci18n_lang_release(&ci18n_ctx.languages[i]);
    }

    memset(&ci18n_ctx, 0, sizeof(ci18n_context_t));
}

CI18N_DEF bool ci18n_load_language(const char *language_code, const char *filepath)
{
    FILE *file;
    char line[CI18N_MAX_LINE_LENGTH];
    ci18n_language_t *lang;
    size_t line_number = 0;

    if (!ci18n_ctx.initialized)
    {
        return ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !filepath)
    {
        return ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
    }

    if (!ci18n_code_fits(language_code))
    {
        return ci18n_fail(CI18N_ERR_CODE_TOO_LONG);
    }

    file = fopen(filepath, "r");
    if (!file)
    {
        return ci18n_fail(CI18N_ERR_FILE_NOT_FOUND);
    }

    /* Created only after the file opened, so a missing file leaves no empty
     * language behind. */
    lang = ci18n_get_or_create_language(language_code);
    if (!lang)
    {
        fclose(file);
        return false; /* get_or_create already recorded why */
    }

    ci18n_reset_load_stats();

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

            ci18n_ctx.load_stats.lines_truncated++;

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

        ci18n_record_line(ci18n_parse_line(lang, line), line_number);
    }

    fclose(file);
    return ci18n_finish_load();
}

CI18N_DEF bool ci18n_load_from_buffer(const char *language_code, const char *buffer, size_t length)
{
    ci18n_language_t *lang;
    char line[CI18N_MAX_LINE_LENGTH];
    size_t pos = 0;
    size_t line_pos = 0;
    size_t line_number = 0;
    bool line_cut = false;

    if (!ci18n_ctx.initialized)
    {
        return ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !buffer)
    {
        return ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
    }

    if (!ci18n_code_fits(language_code))
    {
        return ci18n_fail(CI18N_ERR_CODE_TOO_LONG);
    }

    lang = ci18n_get_or_create_language(language_code);
    if (!lang)
    {
        return false; /* get_or_create already recorded why */
    }

    ci18n_reset_load_stats();

    while (pos < length)
    {
        char c = buffer[pos++];

        if (c == '\n' || c == '\r')
        {
            line[line_pos] = '\0';
            line_number++;
            ci18n_record_line(ci18n_parse_line(lang, line), line_number);
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
                ci18n_ctx.load_stats.lines_truncated++;
                line_cut = true;
            }
        }
    }

    /* Process last line if no newline at end */
    if (line_pos > 0)
    {
        line[line_pos] = '\0';
        line_number++;
        ci18n_record_line(ci18n_parse_line(lang, line), line_number);
    }

    return ci18n_finish_load();
}

CI18N_DEF bool ci18n_set_current(const char *language_code)
{
    if (!ci18n_ctx.initialized)
    {
        return ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code)
    {
        return ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
    }

    if (ci18n_find_language(language_code) < 0)
    {
        return ci18n_fail(CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    ci18n_copy(ci18n_ctx.current_language, sizeof(ci18n_ctx.current_language), language_code);
    ci18n_succeed();
    return true;
}

CI18N_DEF bool ci18n_set_fallback(const char *language_code)
{
    if (!ci18n_ctx.initialized)
    {
        return ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code)
    {
        return ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
    }

    if (ci18n_find_language(language_code) < 0)
    {
        return ci18n_fail(CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    ci18n_copy(ci18n_ctx.fallback_language, sizeof(ci18n_ctx.fallback_language), language_code);
    ci18n_succeed();
    return true;
}

CI18N_DEF const char *ci18n_get(const char *key)
{
    int lang_idx;
    ci18n_language_t *lang;
    int entry_idx;

    if (!ci18n_ctx.initialized)
    {
        ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
        return NULL;
    }

    if (!key)
    {
        ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
        return NULL;
    }

    /* Try current language */
    if (ci18n_ctx.current_language[0] != '\0')
    {
        lang_idx = ci18n_find_language(ci18n_ctx.current_language);
        if (lang_idx >= 0)
        {
            lang = &ci18n_ctx.languages[lang_idx];
            entry_idx = ci18n_find_entry(lang, key);
            if (entry_idx >= 0)
            {
                ci18n_succeed();
                return ci18n_arena_at(&lang->strings, lang->entries[entry_idx].value);
            }
        }
    }

    /* Try fallback language */
    if (ci18n_ctx.fallback_language[0] != '\0')
    {
        lang_idx = ci18n_find_language(ci18n_ctx.fallback_language);
        if (lang_idx >= 0)
        {
            lang = &ci18n_ctx.languages[lang_idx];
            entry_idx = ci18n_find_entry(lang, key);
            if (entry_idx >= 0)
            {
                ci18n_succeed();
                return ci18n_arena_at(&lang->strings, lang->entries[entry_idx].value);
            }
        }
    }

    ci18n_fail(CI18N_ERR_KEY_NOT_FOUND);
    return NULL;
}

CI18N_DEF const char *ci18n_get_or_key(const char *key)
{
    const char *result = ci18n_get(key);
    return result ? result : key;
}

CI18N_DEF bool ci18n_has(const char *key)
{
    ci18n_error_t before = ci18n_ctx.last_error;
    bool found = ci18n_get(key) != NULL;

    /* Asking is not failing: a miss here must not look like a failed call. */
    if (!found && ci18n_ctx.last_error == CI18N_ERR_KEY_NOT_FOUND)
    {
        ci18n_ctx.last_error = before;
    }

    return found;
}

CI18N_DEF const char *ci18n_get_current(void)
{
    if (!ci18n_ctx.initialized)
    {
        return "";
    }
    return ci18n_ctx.current_language;
}

CI18N_DEF size_t ci18n_get_languages(const char **out, size_t capacity)
{
    size_t i;
    size_t n;

    if (!ci18n_ctx.initialized)
    {
        ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
        return 0;
    }

    ci18n_succeed();

    if (out)
    {
        n = capacity < ci18n_ctx.language_count ? capacity : ci18n_ctx.language_count;
        for (i = 0; i < n; i++)
        {
            out[i] = ci18n_ctx.languages[i].code;
        }
    }

    return ci18n_ctx.language_count;
}

CI18N_DEF bool ci18n_set(const char *language_code, const char *key, const char *value)
{
    ci18n_language_t *lang;
    size_t key_len;
    size_t value_len;

    if (!ci18n_ctx.initialized)
    {
        return ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !key || !value)
    {
        return ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
    }

    if (!ci18n_code_fits(language_code))
    {
        return ci18n_fail(CI18N_ERR_CODE_TOO_LONG);
    }

    lang = ci18n_get_or_create_language(language_code);
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

    if (!ci18n_lang_set(lang, key, key_len, value, value_len))
    {
        return false; /* the storage layer already recorded why */
    }

    ci18n_succeed();
    return true;
}

CI18N_DEF bool ci18n_remove(const char *language_code, const char *key)
{
    int lang_idx;
    ci18n_language_t *lang;
    int entry_idx;

    if (!ci18n_ctx.initialized)
    {
        return ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code || !key)
    {
        return ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
    }

    lang_idx = ci18n_find_language(language_code);
    if (lang_idx < 0)
    {
        return ci18n_fail(CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    lang = &ci18n_ctx.languages[lang_idx];
    entry_idx = ci18n_find_entry(lang, key);

    if (entry_idx < 0)
    {
        return ci18n_fail(CI18N_ERR_KEY_NOT_FOUND);
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

    ci18n_succeed();
    return true;
}

CI18N_DEF bool ci18n_clear(const char *language_code)
{
    int lang_idx;
    ci18n_language_t *lang;

    if (!ci18n_ctx.initialized)
    {
        return ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
    }

    if (!language_code)
    {
        return ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
    }

    lang_idx = ci18n_find_language(language_code);
    if (lang_idx < 0)
    {
        return ci18n_fail(CI18N_ERR_LANGUAGE_NOT_FOUND);
    }

    lang = &ci18n_ctx.languages[lang_idx];

    /* Clear current/fallback if being cleared */
    if (strcmp(ci18n_ctx.current_language, language_code) == 0)
    {
        ci18n_ctx.current_language[0] = '\0';
    }
    if (strcmp(ci18n_ctx.fallback_language, language_code) == 0)
    {
        ci18n_ctx.fallback_language[0] = '\0';
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

    ci18n_succeed();
    return true;
}

CI18N_DEF size_t ci18n_count(const char *language_code)
{
    int lang_idx;

    if (!ci18n_ctx.initialized)
    {
        ci18n_fail(CI18N_ERR_NOT_INITIALIZED);
        return 0;
    }

    if (!language_code)
    {
        ci18n_fail(CI18N_ERR_INVALID_ARGUMENT);
        return 0;
    }

    lang_idx = ci18n_find_language(language_code);
    if (lang_idx < 0)
    {
        ci18n_fail(CI18N_ERR_LANGUAGE_NOT_FOUND);
        return 0;
    }

    ci18n_succeed();
    return ci18n_ctx.languages[lang_idx].count;
}

CI18N_DEF bool ci18n_is_initialized(void)
{
    return ci18n_ctx.initialized;
}

CI18N_DEF ci18n_error_t ci18n_last_error(void)
{
    return ci18n_ctx.last_error;
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
    return &ci18n_ctx.load_stats;
}

#ifdef CI18N_THREAD_LOCAL_CONTEXT
CI18N_DEF ci18n_context_t *ci18n_get_context(void)
{
    return &ci18n_ctx;
}
#endif

#endif /* CI18N_IMPLEMENTATION */
