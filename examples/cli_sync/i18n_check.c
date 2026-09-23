/*
 * i18n_check: compare translation files against a reference language.
 *
 *   i18n_check locales/en.txt locales/ru.txt locales/de.txt
 *
 * The first file is the reference. For every other file it reports:
 *
 *   - lines the loader could not parse
 *   - keys the reference has and the translation lacks
 *   - plural forms the language needs and the file does not have
 *   - placeholders that differ from the reference, like {nmae} for {name}
 *   - stale keys the reference no longer has
 *
 * Exit status: 0 when every file is clean, 1 when any has a problem, 2 when a
 * file could not be read. That makes it a CI step as is.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------
 * A growable table of owned key and value strings
 * ------------------------------------------------------------------------ */

typedef struct
{
    char **keys;
    char **values;
    size_t count;
    size_t capacity;
} table_t;

static char *copy_string(const char *s, size_t len)
{
    char *out = (char *)malloc(len + 1);

    if (out)
    {
        memcpy(out, s, len);
        out[len] = '\0';
    }
    return out;
}

static const char *table_find(const table_t *t, const char *key)
{
    size_t i;

    for (i = 0; i < t->count; i++)
    {
        if (strcmp(t->keys[i], key) == 0)
        {
            return t->values[i];
        }
    }
    return NULL;
}

/* Adds a row unless the key is already there. False only when out of memory. */
static bool table_add(table_t *t, const char *key, size_t key_len, const char *value)
{
    char *k;
    char *v;

    k = copy_string(key, key_len);
    if (!k)
    {
        return false;
    }
    if (table_find(t, k))
    {
        free(k);
        return true;
    }
    v = copy_string(value, strlen(value));
    if (!v)
    {
        free(k);
        return false;
    }
    if (t->count == t->capacity)
    {
        size_t cap = t->capacity ? t->capacity * 2 : 16;
        char **keys = (char **)realloc(t->keys, cap * sizeof(char *));
        char **values;

        if (keys)
        {
            t->keys = keys;
        }
        values = (char **)realloc(t->values, cap * sizeof(char *));
        if (values)
        {
            t->values = values;
        }
        if (!keys || !values)
        {
            free(k);
            free(v);
            return false;
        }
        t->capacity = cap;
    }
    t->keys[t->count] = k;
    t->values[t->count] = v;
    t->count++;
    return true;
}

static void table_free(table_t *t)
{
    size_t i;

    for (i = 0; i < t->count; i++)
    {
        free(t->keys[i]);
        free(t->values[i]);
    }
    free(t->keys);
    free(t->values);
    memset(t, 0, sizeof(*t));
}

/* ci18n_foreach visits entries in no particular order, and a report should
 * read the same on every run, so rows are sorted by key before printing. */
static void table_sort(table_t *t)
{
    size_t i;
    size_t j;

    for (i = 1; i < t->count; i++)
    {
        for (j = i; j > 0 && strcmp(t->keys[j - 1], t->keys[j]) > 0; j--)
        {
            char *k = t->keys[j];
            char *v = t->values[j];

            t->keys[j] = t->keys[j - 1];
            t->values[j] = t->values[j - 1];
            t->keys[j - 1] = k;
            t->values[j - 1] = v;
        }
    }
}

/* "photos[one]" gives "photos"; a plain key gives itself. */
static size_t base_length(const char *key)
{
    const char *open = strchr(key, '[');

    return open ? (size_t)(open - key) : strlen(key);
}

/* Two walks over a language: every stored key, and the keys as a translator
 * thinks of them, with the plural forms of one word folded together. */
static bool collect_entry(const char *key, const char *value, void *user_data)
{
    return table_add((table_t *)user_data, key, strlen(key), value);
}

static bool collect_base_key(const char *key, const char *value, void *user_data)
{
    (void)value;
    return table_add((table_t *)user_data, key, base_length(key), "");
}

/* ------------------------------------------------------------------------
 * Checks
 * ------------------------------------------------------------------------ */

/* The placeholder names in a value, sorted and joined: "{album}{count}".
 * A formatter part is dropped, so "{when:date}" counts as "{when}". */
static void placeholder_signature(const char *value, char *out, size_t capacity)
{
    table_t names = {NULL, NULL, 0, 0};
    const char *p = value;
    size_t used = 0;
    size_t i;

    while ((p = strchr(p, '{')) != NULL)
    {
        const char *end = strchr(p, '}');

        if (!end)
        {
            break;
        }
        (void)table_add(&names, p + 1, strcspn(p + 1, ":}"), "");
        p = end + 1;
    }

    table_sort(&names);
    out[0] = '\0';
    for (i = 0; i < names.count; i++)
    {
        size_t len = strlen(names.keys[i]);

        if (used + len + 3 > capacity)
        {
            break;
        }
        out[used++] = '{';
        memcpy(out + used, names.keys[i], len);
        used += len;
        out[used++] = '}';
        out[used] = '\0';
    }
    table_free(&names);
}

/*
 * The plural categories a language uses for whole numbers, as a bit mask.
 *
 * There is no table of these to ask for, and none is needed: ask about
 * enough numbers and every category turns up. 0 to 200 is enough for every
 * integer rule CLDR has.
 */
static unsigned needed_categories(const char *language)
{
    unsigned mask = 0;
    long n;

    for (n = 0; n <= 200; n++)
    {
        mask |= 1u << ci18n_plural_category(language, n);
    }
    return mask;
}

/* The language code from a path: "locales/ru.txt" gives "ru". */
static void code_from_path(const char *path, char *out, size_t capacity)
{
    const char *name = path;
    const char *p;
    size_t len;

    for (p = path; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            name = p + 1;
        }
    }
    len = strcspn(name, ".");
    if (len >= capacity)
    {
        len = capacity - 1;
    }
    memcpy(out, name, len);
    out[len] = '\0';
}

/* Loads one file and reports what the loader could not use. Returns the
 * number of problems, or -1 when the file could not be read at all. */
static int load(ci18n_t *cat, const char *code, const char *path)
{
    const ci18n_load_stats_t *st;
    int problems = 0;

    if (!ci18n_load_language_in(cat, code, path))
    {
        printf("  cannot read %s: %s\n", path,
               ci18n_error_string(ci18n_last_error_in(cat)));
        return -1;
    }

    /* A load that could read the file returns true whatever was in it. The
     * stats say what it actually did. */
    st = ci18n_last_load_stats_in(cat);
    if (st->lines_malformed > 0)
    {
        printf("  %s:%u: no '=' on the line, %u such line(s) skipped\n", path,
               (unsigned)st->first_malformed_line, (unsigned)st->lines_malformed);
        problems++;
    }
    if (st->keys_truncated + st->values_truncated + st->lines_truncated > 0)
    {
        printf("  %s: text was cut to fit the build's limits\n", path);
        problems++;
    }
    return problems;
}

/* Compares one translation value with the reference value it translates. */
static int compare_placeholders(const char *form, const char *value,
                                const char *ref_value, const char *ref)
{
    char want[CI18N_MAX_VALUE_LENGTH];
    char have[CI18N_MAX_VALUE_LENGTH];

    placeholder_signature(ref_value, want, sizeof(want));
    placeholder_signature(value, have, sizeof(have));
    if (strcmp(want, have) == 0)
    {
        return 0;
    }
    printf("  placeholders: %s has %s, %s has %s\n", form,
           have[0] ? have : "none", ref, want[0] ? want : "none");
    return 1;
}

static int check(ci18n_t *cat, const char *ref, const table_t *ref_entries,
                 const table_t *ref_keys, const char *lang, const char *path)
{
    static const char *const category_names[] = {"zero", "one", "two",
                                                 "few", "many", "other"};
    table_t entries = {NULL, NULL, 0, 0};
    table_t keys = {NULL, NULL, 0, 0};
    char form[CI18N_MAX_KEY_LENGTH + 8];
    char shown[CI18N_MAX_VALUE_LENGTH];
    unsigned needed = needed_categories(lang);
    size_t present = 0;
    size_t i;
    int problems;

    printf("%s:\n", lang);
    problems = load(cat, lang, path);
    if (problems < 0)
    {
        return -1;
    }

    ci18n_foreach_in(cat, lang, collect_entry, &entries);
    ci18n_foreach_in(cat, lang, collect_base_key, &keys);
    table_sort(&keys);

    for (i = 0; i < ref_keys->count; i++)
    {
        const char *key = ref_keys->keys[i];
        const char *ref_value = table_find(ref_entries, key);
        int c;

        if (!table_find(&keys, key))
        {
            /* Users still see something: the reference text, through the
             * fallback a real program sets. Show them what. */
            ci18n_set_current_in(cat, lang);
            ci18n_set_fallback_in(cat, ref);
            if (ref_value)
            {
                ci18n_get_copy_in(cat, key, shown, sizeof(shown));
            }
            else
            {
                ci18n_format_plural_in(cat, shown, sizeof(shown), key, 2, NULL);
            }
            printf("  missing: %s, users see \"%s\"\n", key, shown);
            problems++;
            continue;
        }
        present++;

        if (ref_value)
        {
            const char *value = table_find(&entries, key);

            problems += value ? compare_placeholders(key, value, ref_value, ref) : 0;
            continue;
        }

        /* A plural key. The language decides which forms it needs, not the
         * reference: Russian needs three where English has two. */
        (void)snprintf(form, sizeof(form), "%s[other]", key);
        ref_value = table_find(ref_entries, form);

        for (c = CI18N_PLURAL_ZERO; c <= CI18N_PLURAL_OTHER; c++)
        {
            const char *value;

            if (!(needed & (1u << c)))
            {
                continue;
            }
            (void)snprintf(form, sizeof(form), "%s[%s]", key, category_names[c]);
            value = table_find(&entries, form);
            if (!value)
            {
                printf("  missing form: %s\n", form);
                problems++;
            }
            else if (ref_value)
            {
                problems += compare_placeholders(form, value, ref_value, ref);
            }
        }
    }

    for (i = 0; i < keys.count; i++)
    {
        if (!table_find(ref_keys, keys.keys[i]))
        {
            printf("  stale: %s is gone from %s\n", keys.keys[i], ref);
            problems++;
        }
    }

    printf("  %u of %u keys, %s\n", (unsigned)present, (unsigned)ref_keys->count,
           problems ? "needs work" : "ok");
    table_free(&entries);
    table_free(&keys);
    return problems;
}

int main(int argc, char **argv)
{
    table_t ref_entries = {NULL, NULL, 0, 0};
    table_t ref_keys = {NULL, NULL, 0, 0};
    char ref[CI18N_MAX_CODE_LENGTH];
    ci18n_t *cat;
    int status = 0;
    int i;

    if (argc < 3)
    {
        fprintf(stderr, "usage: %s REFERENCE TRANSLATION...\n", argv[0]);
        return 2;
    }

    /* A catalogue of its own rather than the global one. Nothing else in a
     * tool this size would care, but it is the habit worth showing. */
    cat = ci18n_create();
    if (!cat)
    {
        fprintf(stderr, "out of memory\n");
        return 2;
    }

    code_from_path(argv[1], ref, sizeof(ref));
    printf("%s (reference):\n", ref);
    if (load(cat, ref, argv[1]) != 0)
    {
        ci18n_destroy(cat);
        return 2;
    }
    ci18n_foreach_in(cat, ref, collect_entry, &ref_entries);
    ci18n_foreach_in(cat, ref, collect_base_key, &ref_keys);
    table_sort(&ref_keys);
    printf("  %u keys\n", (unsigned)ref_keys.count);

    for (i = 2; i < argc; i++)
    {
        char lang[CI18N_MAX_CODE_LENGTH];
        int problems;

        code_from_path(argv[i], lang, sizeof(lang));
        problems = check(cat, ref, &ref_entries, &ref_keys, lang, argv[i]);
        if (problems < 0)
        {
            status = 2;
        }
        else if (problems > 0 && status == 0)
        {
            status = 1;
        }
    }

    table_free(&ref_entries);
    table_free(&ref_keys);
    ci18n_destroy(cat);
    return status;
}
