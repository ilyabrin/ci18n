/*
 * ci18n.h - v1.0.0
 * Single-header internationalization (i18n) library for C projects
 *
 * Features:
 *   - Simple translation key-value storage
 *   - Multiple language support
 *   - UTF-8 string handling, BOM tolerant
 *   - Thread-local context option
 *   - No external dependencies, C99 and newer
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

/* ============================================================================
 * Version
 * ============================================================================ */

#define CI18N_VERSION_MAJOR 1
#define CI18N_VERSION_MINOR 0
#define CI18N_VERSION_PATCH 0
#define CI18N_VERSION_STRING "1.0.0"

/* Compare against this to require a minimum version at compile time:
 *   #if CI18N_VERSION < CI18N_VERSION_NUMBER(1, 1, 0)
 *   #error "ci18n 1.1.0 or newer is required"
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

#ifndef CI18N_MAX_LINE_LENGTH
#define CI18N_MAX_LINE_LENGTH 4096
#endif

/* Includes the terminator, so the default fits a 31 character code. Long
 * enough for anything BCP 47 produces in practice, such as ca-ES-valencia. */
#ifndef CI18N_MAX_CODE_LENGTH
#define CI18N_MAX_CODE_LENGTH 32
#endif

    /* ============================================================================
     * Types
     * ============================================================================ */

    typedef struct ci18n_entry
    {
        char key[CI18N_MAX_KEY_LENGTH];
        char value[CI18N_MAX_VALUE_LENGTH];
    } ci18n_entry_t;

    typedef struct ci18n_language
    {
        char code[CI18N_MAX_CODE_LENGTH];
        ci18n_entry_t *entries;
        size_t count;
        size_t capacity;
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
     *   ci18n_set(), ci18n_remove(), ci18n_load_language() and
     *   ci18n_load_from_buffer() may reallocate the entry array
     *
     *   ci18n_clear(), ci18n_free() and ci18n_set_current() invalidate
     *   pointers outright
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

/* Internal helper macros */
#define CI18N_MIN(a, b) ((a) < (b) ? (a) : (b))

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

/* Trim leading and trailing whitespace in-place */
static void ci18n_trim(char *str)
{
    char *start = str;
    char *end;
    size_t len;

    /* Trim leading space */
    while (ci18n_is_space(*start))
        start++;

    /* All spaces? */
    if (*start == 0)
    {
        str[0] = '\0';
        return;
    }

    /* Trim trailing space */
    end = start + strlen(start) - 1;
    while (end > start && ci18n_is_space(*end))
        end--;

    /* Write new null terminator */
    len = (size_t)(end - start + 1);
    memmove(str, start, len);
    str[len] = '\0';
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

/* Find entry by key in language, returns index or -1 if not found */
static int ci18n_find_entry(ci18n_language_t *lang, const char *key)
{
    size_t i;
    for (i = 0; i < lang->count; i++)
    {
        if (strcmp(lang->entries[i].key, key) == 0)
        {
            return (int)i;
        }
    }
    return -1;
}

/* Get or create language */
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

    /* Allocate the initial entries array.
     *
     * Capacity is only published once the allocation succeeds: otherwise
     * ci18n_ensure_capacity() would see room in a NULL array and let the
     * caller write through a null pointer. */
    lang->entries = (ci18n_entry_t *)malloc(sizeof(ci18n_entry_t) * 64);
    if (!lang->entries)
    {
        ci18n_fail(CI18N_ERR_OUT_OF_MEMORY);
        return NULL;
    }

    lang->capacity = 64;
    lang->count = 0;
    ci18n_ctx.language_count++;

    return lang;
}

/* Ensure capacity for entries */
static bool ci18n_ensure_capacity(ci18n_language_t *lang)
{
    ci18n_entry_t *new_entries;
    size_t new_capacity;

    if (lang->count < lang->capacity)
    {
        return true;
    }

    if (lang->count >= CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        return ci18n_fail(CI18N_ERR_TOO_MANY_KEYS);
    }

    new_capacity = lang->capacity * 2;
    if (new_capacity > CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        new_capacity = CI18N_MAX_KEYS_PER_LANGUAGE;
    }

    new_entries = (ci18n_entry_t *)realloc(lang->entries, sizeof(ci18n_entry_t) * new_capacity);
    if (!new_entries)
    {
        return ci18n_fail(CI18N_ERR_OUT_OF_MEMORY);
    }

    lang->entries = new_entries;
    lang->capacity = new_capacity;
    return true;
}

/* Parse a single line */
/* What one line turned into, so a loader can keep count */
typedef enum ci18n_line_result
{
    CI18N_LINE_LOADED,    /* an entry was added or updated */
    CI18N_LINE_SKIPPED,   /* blank line or comment */
    CI18N_LINE_MALFORMED, /* no separator, or an empty key */
    CI18N_LINE_FAILED     /* out of room or out of memory */
} ci18n_line_result_t;

static ci18n_line_result_t ci18n_parse_line(ci18n_language_t *lang, const char *line)
{
    const char *eq;
    char key[CI18N_MAX_KEY_LENGTH];
    char value[CI18N_MAX_VALUE_LENGTH];
    ci18n_entry_t *entry;
    int existing;
    size_t key_len;
    size_t raw_key_len;

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

    /* Extract key, noting if it did not fit */
    raw_key_len = (size_t)(eq - line);
    key_len = CI18N_MIN(raw_key_len, CI18N_MAX_KEY_LENGTH - 1);
    if (key_len < raw_key_len)
    {
        ci18n_ctx.load_stats.keys_truncated++;
    }

    memcpy(key, line, key_len);
    key[key_len] = '\0';
    ci18n_trim(key);

    if (strlen(key) == 0)
    {
        return CI18N_LINE_MALFORMED;
    }

    /* Extract value, noting if it did not fit */
    if (strlen(eq + 1) > CI18N_MAX_VALUE_LENGTH - 1)
    {
        ci18n_ctx.load_stats.values_truncated++;
    }

    ci18n_copy(value, CI18N_MAX_VALUE_LENGTH, eq + 1);
    ci18n_trim(value);

    /* Check if key already exists */
    existing = ci18n_find_entry(lang, key);
    if (existing >= 0)
    {
        /* Update existing entry */
        ci18n_copy(lang->entries[existing].value, CI18N_MAX_VALUE_LENGTH, value);
        return CI18N_LINE_LOADED;
    }

    /* Add new entry */
    if (!ci18n_ensure_capacity(lang))
    {
        return CI18N_LINE_FAILED;
    }

    entry = &lang->entries[lang->count++];
    ci18n_copy(entry->key, CI18N_MAX_KEY_LENGTH, key);
    ci18n_copy(entry->value, CI18N_MAX_VALUE_LENGTH, value);

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
        if (ci18n_ctx.languages[i].entries)
        {
            free(ci18n_ctx.languages[i].entries);
            ci18n_ctx.languages[i].entries = NULL;
        }
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

        /* A line that filled the buffer without a terminator was cut, and its
         * tail will arrive as a separate line on the next read. */
        if (len == sizeof(line) - 1 && line[len - 1] != '\n' && line[len - 1] != '\r')
        {
            ci18n_ctx.load_stats.lines_truncated++;
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
                return lang->entries[entry_idx].value;
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
                return lang->entries[entry_idx].value;
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
    ci18n_entry_t *entry;
    int existing;

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

    /* Check if key already exists */
    existing = ci18n_find_entry(lang, key);
    if (existing >= 0)
    {
        ci18n_copy(lang->entries[existing].value, CI18N_MAX_VALUE_LENGTH, value);
        ci18n_succeed();
        return true;
    }

    /* Add new entry */
    if (!ci18n_ensure_capacity(lang))
    {
        return false; /* ensure_capacity already recorded why */
    }

    entry = &lang->entries[lang->count++];
    ci18n_copy(entry->key, CI18N_MAX_KEY_LENGTH, key);
    ci18n_copy(entry->value, CI18N_MAX_VALUE_LENGTH, value);

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

    /* Shift remaining entries */
    if (entry_idx < (int)(lang->count - 1))
    {
        memmove(&lang->entries[entry_idx],
                &lang->entries[entry_idx + 1],
                sizeof(ci18n_entry_t) * (lang->count - 1 - entry_idx));
    }

    lang->count--;
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

    lang->count = 0;
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
