/*
 * ci18n.h - v1.0.0
 * Single-header internationalization (i18n) library for C projects
 *
 * Features:
 *   - Simple translation key-value storage
 *   - Multiple language support
 *   - UTF-8 string handling
 *   - Thread-safe option
 *   - No external dependencies (pure C)
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
 * Copyright (c) 2026 https://github.com/ilyabrin
 *
 * LICENSE: MIT
 */

#ifndef CI18N_H
#define CI18N_H

#include <stddef.h>
#include <stdbool.h>

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
    bool ci18n_init(void);

    /* Shutdown and free all resources. */
    void ci18n_free(void);

    /*
     * Load translations from a file.
     * File format: key=value (one per line), # for comments
     * Returns: true on success, false on failure
     */
    bool ci18n_load_language(const char *language_code, const char *filepath);

    /*
     * Load translations from a memory buffer.
     * Buffer should contain newline-separated key=value pairs.
     * Returns: true on success, false on failure
     */
    bool ci18n_load_from_buffer(const char *language_code, const char *buffer, size_t length);

    /*
     * Set the current active language.
     * Returns: true if language exists, false otherwise
     */
    bool ci18n_set_current(const char *language_code);

    /*
     * Set the fallback language (used when key not found in current language).
     * Returns: true if language exists, false otherwise
     */
    bool ci18n_set_fallback(const char *language_code);

    /*
     * Get translation for a key in current language.
     * Returns: translation string or NULL if not found
     */
    const char *ci18n_get(const char *key);

    /*
     * Get translation with fallback to key itself if not found.
     * Returns: translation string or the key if not found
     */
    const char *ci18n_get_or_key(const char *key);

    /*
     * Check if a key exists in current language.
     * Returns: true if exists, false otherwise
     */
    bool ci18n_has(const char *key);

    /*
     * Get the current language code.
     * Returns: language code string or empty string if not set
     */
    const char *ci18n_get_current(void);

    /*
     * Get list of available language codes.
     * Returns: pointer to array of language codes, sets count
     */
    const char **ci18n_get_languages(size_t *count);

    /*
     * Add a translation entry programmatically.
     * Returns: true on success, false on failure
     */
    bool ci18n_set(const char *language_code, const char *key, const char *value);

    /*
     * Remove a translation entry.
     * Returns: true if removed, false if not found
     */
    bool ci18n_remove(const char *language_code, const char *key);

    /*
     * Clear all translations for a language.
     * Returns: true if cleared, false if language not found
     */
    bool ci18n_clear(const char *language_code);

    /*
     * Get translation count for a language.
     * Returns: number of entries, or 0 if language not found
     */
    size_t ci18n_count(const char *language_code);

    /*
     * Check if the system is initialized.
     * Returns: true if initialized, false otherwise
     */
    bool ci18n_is_initialized(void);

    /* ============================================================================
     * Optional: Thread-safe version (define CI18N_THREAD_SAFE before including)
     * ============================================================================ */

#ifdef CI18N_THREAD_SAFE
    /* Get thread-local context for manual control */
    ci18n_context_t *ci18n_get_context(void);
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
#include <ctype.h>

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

/* Trim leading and trailing whitespace in-place */
static void ci18n_trim(char *str)
{
    char *start = str;
    char *end;
    size_t len;

    /* Trim leading space */
    while (isspace((unsigned char)*start))
        start++;

    /* All spaces? */
    if (*start == 0)
    {
        str[0] = '\0';
        return;
    }

    /* Trim trailing space */
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end))
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

    lang = &ci18n_ctx.languages[ci18n_ctx.language_count++];
    memset(lang, 0, sizeof(ci18n_language_t));
    strncpy(lang->code, code, sizeof(lang->code) - 1);
    lang->code[sizeof(lang->code) - 1] = '\0';

    /* Allocate initial entries array */
    lang->capacity = 64;
    lang->entries = (ci18n_entry_t *)malloc(sizeof(ci18n_entry_t) * lang->capacity);
    lang->count = 0;

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

    /* Skip empty lines and comments */
    while (*line && isspace((unsigned char)*line))
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
    strncpy(value, eq + 1, CI18N_MAX_VALUE_LENGTH - 1);
    value[CI18N_MAX_VALUE_LENGTH - 1] = '\0';
    ci18n_trim(value);

    /* Check if key already exists */
    existing = ci18n_find_entry(lang, key);
    if (existing >= 0)
    {
        /* Update existing entry */
        strncpy(lang->entries[existing].value, value, CI18N_MAX_VALUE_LENGTH - 1);
        return true;
    }

    /* Add new entry */
    if (!ci18n_ensure_capacity(lang))
    {
        return false;
    }

    entry = &lang->entries[lang->count++];
    strncpy(entry->key, key, CI18N_MAX_KEY_LENGTH - 1);
    entry->key[CI18N_MAX_KEY_LENGTH - 1] = '\0';
    strncpy(entry->value, value, CI18N_MAX_VALUE_LENGTH - 1);
    entry->value[CI18N_MAX_VALUE_LENGTH - 1] = '\0';

    return true;
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

bool ci18n_init(void)
{
    if (ci18n_ctx.initialized)
    {
        return true;
    }

    memset(&ci18n_ctx, 0, sizeof(ci18n_context_t));
    ci18n_ctx.initialized = true;
    return true;
}

void ci18n_free(void)
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

bool ci18n_load_language(const char *language_code, const char *filepath)
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
        /* Remove newline */
        size_t len = strlen(line);
        if (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
        {
            line[len - 1] = '\0';
            if (len > 1 && line[len - 2] == '\r')
            {
                line[len - 2] = '\0';
            }
        }
        ci18n_parse_line(lang, line);
    }

    fclose(file);
    return true;
}

bool ci18n_load_from_buffer(const char *language_code, const char *buffer, size_t length)
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

bool ci18n_set_current(const char *language_code)
{
    if (!ci18n_ctx.initialized || !language_code)
    {
        return false;
    }

    if (ci18n_find_language(language_code) < 0)
    {
        return false;
    }

    strncpy(ci18n_ctx.current_language, language_code, sizeof(ci18n_ctx.current_language) - 1);
    ci18n_ctx.current_language[sizeof(ci18n_ctx.current_language) - 1] = '\0';
    return true;
}

bool ci18n_set_fallback(const char *language_code)
{
    if (!ci18n_ctx.initialized || !language_code)
    {
        return false;
    }

    if (ci18n_find_language(language_code) < 0)
    {
        return false;
    }

    strncpy(ci18n_ctx.fallback_language, language_code, sizeof(ci18n_ctx.fallback_language) - 1);
    ci18n_ctx.fallback_language[sizeof(ci18n_ctx.fallback_language) - 1] = '\0';
    return true;
}

const char *ci18n_get(const char *key)
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

const char *ci18n_get_or_key(const char *key)
{
    const char *result = ci18n_get(key);
    return result ? result : key;
}

bool ci18n_has(const char *key)
{
    return ci18n_get(key) != NULL;
}

const char *ci18n_get_current(void)
{
    if (!ci18n_ctx.initialized)
    {
        return "";
    }
    return ci18n_ctx.current_language;
}

const char **ci18n_get_languages(size_t *count)
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

bool ci18n_set(const char *language_code, const char *key, const char *value)
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
        strncpy(lang->entries[existing].value, value, CI18N_MAX_VALUE_LENGTH - 1);
        return true;
    }

    /* Add new entry */
    if (!ci18n_ensure_capacity(lang))
    {
        return false;
    }

    entry = &lang->entries[lang->count++];
    strncpy(entry->key, key, CI18N_MAX_KEY_LENGTH - 1);
    entry->key[CI18N_MAX_KEY_LENGTH - 1] = '\0';
    strncpy(entry->value, value, CI18N_MAX_VALUE_LENGTH - 1);
    entry->value[CI18N_MAX_VALUE_LENGTH - 1] = '\0';

    return true;
}

bool ci18n_remove(const char *language_code, const char *key)
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

bool ci18n_clear(const char *language_code)
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

size_t ci18n_count(const char *language_code)
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

bool ci18n_is_initialized(void)
{
    return ci18n_ctx.initialized;
}

#ifdef CI18N_THREAD_SAFE
ci18n_context_t *ci18n_get_context(void)
{
    return &ci18n_ctx;
}
#endif

#endif /* CI18N_IMPLEMENTATION */
