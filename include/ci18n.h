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
        char code[16];
        ci18n_entry_t *entries;
        size_t count;
        size_t capacity;
    } ci18n_language_t;

    typedef struct ci18n_context
    {
        ci18n_language_t languages[CI18N_MAX_LANGUAGES];
        size_t language_count;
        char current_language[16];
        char fallback_language[16];
        bool initialized;
    } ci18n_context_t;

    /* ============================================================================
     * Public API
     * ============================================================================ */

    /* Initialize the i18n system. Must be called before any other function. */
    CI18N_DEF bool ci18n_init(void);

    /* Shutdown and free all resources. */
    CI18N_DEF void ci18n_free(void);

    /*
     * Load translations from a file.
     * File format: key=value (one per line), # for comments
     * Returns: true on success, false on failure
     */
    CI18N_DEF bool ci18n_load_language(const char *language_code, const char *filepath);

    /*
     * Load translations from a memory buffer.
     * Buffer should contain newline-separated key=value pairs.
     * Returns: true on success, false on failure
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
     * Get translation for a key in current language.
     * Returns: translation string or NULL if not found
     */
    CI18N_DEF const char *ci18n_get(const char *key);

    /*
     * Get translation with fallback to key itself if not found.
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
     * Returns: language code string or empty string if not set
     */
    CI18N_DEF const char *ci18n_get_current(void);

    /*
     * Get list of available language codes.
     * Returns: pointer to array of language codes, sets count
     */
    CI18N_DEF const char **ci18n_get_languages(size_t *count);

    /*
     * Add a translation entry programmatically.
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
     * Optional: Thread-safe version (define CI18N_THREAD_SAFE before including)
     * ============================================================================ */

#ifdef CI18N_THREAD_SAFE
    /* Get thread-local context for manual control */
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

#ifdef CI18N_THREAD_SAFE
#ifdef _WIN32
#include <windows.h>
#define CI18N_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
#define CI18N_THREAD_LOCAL __thread
#else
#define CI18N_THREAD_LOCAL _Thread_local
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
        return false;
    }

    new_capacity = lang->capacity * 2;
    if (new_capacity > CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        new_capacity = CI18N_MAX_KEYS_PER_LANGUAGE;
    }

    new_entries = (ci18n_entry_t *)realloc(lang->entries, sizeof(ci18n_entry_t) * new_capacity);
    if (!new_entries)
    {
        return false;
    }

    lang->entries = new_entries;
    lang->capacity = new_capacity;
    return true;
}

/* Parse a single line */
static bool ci18n_parse_line(ci18n_language_t *lang, const char *line)
{
    const char *eq;
    char key[CI18N_MAX_KEY_LENGTH];
    char value[CI18N_MAX_VALUE_LENGTH];
    ci18n_entry_t *entry;
    int existing;
    size_t key_len;

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
        return true;
    }

    /* Find equals sign */
    eq = strchr(line, '=');
    if (!eq)
    {
        return false;
    }

    /* Extract key */
    key_len = CI18N_MIN((size_t)(eq - line), CI18N_MAX_KEY_LENGTH - 1);
    memcpy(key, line, key_len);
    key[key_len] = '\0';
    ci18n_trim(key);

    if (strlen(key) == 0)
    {
        return false;
    }

    /* Extract value */
    ci18n_copy(value, CI18N_MAX_VALUE_LENGTH, eq + 1);
    ci18n_trim(value);

    /* Check if key already exists */
    existing = ci18n_find_entry(lang, key);
    if (existing >= 0)
    {
        /* Update existing entry */
        ci18n_copy(lang->entries[existing].value, CI18N_MAX_VALUE_LENGTH, value);
        return true;
    }

    /* Add new entry */
    if (!ci18n_ensure_capacity(lang))
    {
        return false;
    }

    entry = &lang->entries[lang->count++];
    ci18n_copy(entry->key, CI18N_MAX_KEY_LENGTH, key);
    ci18n_copy(entry->value, CI18N_MAX_VALUE_LENGTH, value);

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

    if (!ci18n_ctx.initialized || !language_code || !filepath)
    {
        return false;
    }

    file = fopen(filepath, "r");
    if (!file)
    {
        return false;
    }

    lang = ci18n_get_or_create_language(language_code);
    if (!lang)
    {
        fclose(file);
        return false;
    }

    while (fgets(line, sizeof(line), file))
    {
        /* Strip the line terminator. One loop covers LF, CRLF and a lone CR,
         * so a file authored on any platform parses the same way. */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[--len] = '\0';
        }

        ci18n_parse_line(lang, line);
    }

    fclose(file);
    return true;
}

CI18N_DEF bool ci18n_load_from_buffer(const char *language_code, const char *buffer, size_t length)
{
    ci18n_language_t *lang;
    char line[CI18N_MAX_LINE_LENGTH];
    size_t pos = 0;
    size_t line_pos = 0;

    if (!ci18n_ctx.initialized || !language_code || !buffer)
    {
        return false;
    }

    lang = ci18n_get_or_create_language(language_code);
    if (!lang)
    {
        return false;
    }

    while (pos < length)
    {
        char c = buffer[pos++];

        if (c == '\n' || c == '\r')
        {
            line[line_pos] = '\0';
            ci18n_parse_line(lang, line);
            line_pos = 0;

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
        }
    }

    /* Process last line if no newline at end */
    if (line_pos > 0)
    {
        line[line_pos] = '\0';
        ci18n_parse_line(lang, line);
    }

    return true;
}

CI18N_DEF bool ci18n_set_current(const char *language_code)
{
    if (!ci18n_ctx.initialized || !language_code)
    {
        return false;
    }

    if (ci18n_find_language(language_code) < 0)
    {
        return false;
    }

    ci18n_copy(ci18n_ctx.current_language, sizeof(ci18n_ctx.current_language), language_code);
    return true;
}

CI18N_DEF bool ci18n_set_fallback(const char *language_code)
{
    if (!ci18n_ctx.initialized || !language_code)
    {
        return false;
    }

    if (ci18n_find_language(language_code) < 0)
    {
        return false;
    }

    ci18n_copy(ci18n_ctx.fallback_language, sizeof(ci18n_ctx.fallback_language), language_code);
    return true;
}

CI18N_DEF const char *ci18n_get(const char *key)
{
    int lang_idx;
    ci18n_language_t *lang;
    int entry_idx;

    if (!ci18n_ctx.initialized || !key)
    {
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
                return lang->entries[entry_idx].value;
            }
        }
    }

    return NULL;
}

CI18N_DEF const char *ci18n_get_or_key(const char *key)
{
    const char *result = ci18n_get(key);
    return result ? result : key;
}

CI18N_DEF bool ci18n_has(const char *key)
{
    return ci18n_get(key) != NULL;
}

CI18N_DEF const char *ci18n_get_current(void)
{
    if (!ci18n_ctx.initialized)
    {
        return "";
    }
    return ci18n_ctx.current_language;
}

CI18N_DEF const char **ci18n_get_languages(size_t *count)
{
    static const char *codes[CI18N_MAX_LANGUAGES];
    size_t i;

    if (!count)
    {
        return NULL;
    }

    *count = ci18n_ctx.language_count;

    for (i = 0; i < ci18n_ctx.language_count; i++)
    {
        codes[i] = ci18n_ctx.languages[i].code;
    }

    return codes;
}

CI18N_DEF bool ci18n_set(const char *language_code, const char *key, const char *value)
{
    ci18n_language_t *lang;
    ci18n_entry_t *entry;
    int existing;

    if (!ci18n_ctx.initialized || !language_code || !key || !value)
    {
        return false;
    }

    lang = ci18n_get_or_create_language(language_code);
    if (!lang)
    {
        return false;
    }

    /* Check if key already exists */
    existing = ci18n_find_entry(lang, key);
    if (existing >= 0)
    {
        ci18n_copy(lang->entries[existing].value, CI18N_MAX_VALUE_LENGTH, value);
        return true;
    }

    /* Add new entry */
    if (!ci18n_ensure_capacity(lang))
    {
        return false;
    }

    entry = &lang->entries[lang->count++];
    ci18n_copy(entry->key, CI18N_MAX_KEY_LENGTH, key);
    ci18n_copy(entry->value, CI18N_MAX_VALUE_LENGTH, value);

    return true;
}

CI18N_DEF bool ci18n_remove(const char *language_code, const char *key)
{
    int lang_idx;
    ci18n_language_t *lang;
    int entry_idx;

    if (!ci18n_ctx.initialized || !language_code || !key)
    {
        return false;
    }

    lang_idx = ci18n_find_language(language_code);
    if (lang_idx < 0)
    {
        return false;
    }

    lang = &ci18n_ctx.languages[lang_idx];
    entry_idx = ci18n_find_entry(lang, key);

    if (entry_idx < 0)
    {
        return false;
    }

    /* Shift remaining entries */
    if (entry_idx < (int)(lang->count - 1))
    {
        memmove(&lang->entries[entry_idx],
                &lang->entries[entry_idx + 1],
                sizeof(ci18n_entry_t) * (lang->count - 1 - entry_idx));
    }

    lang->count--;
    return true;
}

CI18N_DEF bool ci18n_clear(const char *language_code)
{
    int lang_idx;
    ci18n_language_t *lang;

    if (!ci18n_ctx.initialized || !language_code)
    {
        return false;
    }

    lang_idx = ci18n_find_language(language_code);
    if (lang_idx < 0)
    {
        return false;
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
    return true;
}

CI18N_DEF size_t ci18n_count(const char *language_code)
{
    int lang_idx;

    if (!ci18n_ctx.initialized || !language_code)
    {
        return 0;
    }

    lang_idx = ci18n_find_language(language_code);
    if (lang_idx < 0)
    {
        return 0;
    }

    return ci18n_ctx.languages[lang_idx].count;
}

CI18N_DEF bool ci18n_is_initialized(void)
{
    return ci18n_ctx.initialized;
}

#ifdef CI18N_THREAD_SAFE
CI18N_DEF ci18n_context_t *ci18n_get_context(void)
{
    return &ci18n_ctx;
}
#endif

#endif /* CI18N_IMPLEMENTATION */
