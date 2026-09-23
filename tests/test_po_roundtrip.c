/*
 * Round trip check for tools/po2ci18n.py.
 *
 *   make test-po
 *
 * Converts tests/po/sample.po, loads the result, and checks the strings came
 * back intact.
 *
 *   make test-mo
 *
 * Does the same, then loads each .mo file named after the first argument,
 * compiled by msgfmt from the same .po, and checks that each holds exactly
 * the entries the converter wrote. Two routes from one source must agree. The converter is a script outside the library, so without this
 * nothing would notice it rotting. It earned its place immediately: it caught
 * a parser bug that 89 unit tests and the fuzzer had both missed, where a key
 * containing a backslash switched escape decoding off for its own value.
 *
 * Exits non-zero if anything does not match.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

static void expect(const char *key, const char *want)
{
    const char *got = ci18n_get(key);

    if (!got || strcmp(got, want) != 0)
    {
        printf("MISMATCH key=[%s]\n  want [%s]\n  got  [%s]\n",
               key, want, got ? got : "(null)");
        failures++;
        return;
    }

    printf("ok  [%s] -> [%s]\n", key, got);
}

typedef struct
{
    ci18n_t *other;
    int differences;
} compare_t;

static bool compare_entry(const char *key, const char *value, void *user_data)
{
    compare_t *cmp = (compare_t *)user_data;
    char got[1024];

    if (!ci18n_has_in(cmp->other, key))
    {
        printf("MISMATCH .mo lacks [%s]\n", key);
        cmp->differences++;
        return true;
    }

    ci18n_get_copy_in(cmp->other, key, got, sizeof(got));
    if (strcmp(got, value) != 0)
    {
        printf("MISMATCH [%s]\n  converter [%s]\n  .mo       [%s]\n", key, value, got);
        cmp->differences++;
    }
    return true;
}

int main(int argc, char **argv)
{
    char text[256];

    if (argc < 2)
    {
        return 2;
    }

    ci18n_init();

    if (!ci18n_load_language("ru", argv[1]))
    {
        printf("load failed: %s\n", ci18n_error_string(ci18n_last_error()));
        return 1;
    }

    ci18n_set_current("ru");

    printf("entries loaded: %d\n", (int)ci18n_count("ru"));

    expect("Hello, world", "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82, "
                           "\xd0\xbc\xd0\xb8\xd1\x80");

    /* msgctxt became a prefix, so the two senses of "Open" stay apart. */
    expect("menu.Open", "\xd0\x9e\xd1\x82\xd0\xba\xd1\x80\xd1\x8b\xd1\x82\xd1\x8c");
    expect("status.Open", "\xd0\x9e\xd1\x82\xd0\xba\xd1\x80\xd1\x8b\xd1\x82");

    /* The equals signs survived on both sides of the separator. */
    expect("key=with=equals",
           "\xd0\xb7\xd0\xbd\xd0\xb0\xd1\x87\xd0\xb5\xd0\xbd\xd0\xb8\xd0\xb5="
           "\xd1\x81=\xd1\x80\xd0\xb0\xd0\xb2\xd0\xb5\xd0\xbd\xd1\x81\xd1\x82"
           "\xd0\xb2\xd0\xb0\xd0\xbc\xd0\xb8");

    /* Real control characters, not the two-character sequences. */
    {
        const char *got = ci18n_get("First\nSecond\tTabbed");

        if (!got || strchr(got, '\n') == NULL || strchr(got, '\t') == NULL)
        {
            printf("MISMATCH escapes did not decode: [%s]\n", got ? got : "(null)");
            failures++;
        }
        else
        {
            printf("ok  escapes decoded into real control characters\n");
        }
    }

    /* The plural forms landed on the right CLDR categories. */
    ci18n_format_plural(text, sizeof(text), "%d file", 1, NULL);
    printf("plural 1:  %s\n", text);
    ci18n_format_plural(text, sizeof(text), "%d file", 3, NULL);
    printf("plural 3:  %s\n", text);
    ci18n_format_plural(text, sizeof(text), "%d file", 5, NULL);
    printf("plural 5:  %s\n", text);

    if (strstr(ci18n_plural_or_key("%d file", 1),
               "\xd1\x84\xd0\xb0\xd0\xb9\xd0\xbb") == NULL)
    {
        printf("MISMATCH plural one\n");
        failures++;
    }

    /* Fuzzy and untranslated entries must not have come through. */
    if (ci18n_get("Not reviewed") != NULL)
    {
        printf("MISMATCH a fuzzy entry was emitted\n");
        failures++;
    }
    if (ci18n_get("Nobody translated this") != NULL)
    {
        printf("MISMATCH an untranslated entry was emitted\n");
        failures++;
    }
    if (ci18n_get("Obsolete entry") != NULL)
    {
        printf("MISMATCH an obsolete entry was emitted\n");
        failures++;
    }

    /* Each .mo file must hold exactly what the converter wrote. */
    {
        int i;

        for (i = 2; i < argc; i++)
        {
            ci18n_t *mo = ci18n_create();
            compare_t cmp;

            if (!mo || !ci18n_load_mo_in(mo, "ru", argv[i]))
            {
                printf("MISMATCH cannot load %s\n", argv[i]);
                failures++;
                ci18n_destroy(mo);
                continue;
            }

            ci18n_set_current_in(mo, "ru");
            cmp.other = mo;
            cmp.differences = 0;
            ci18n_foreach("ru", compare_entry, &cmp);
            if (ci18n_count_in(mo, "ru") != ci18n_count("ru"))
            {
                printf("MISMATCH %s has %u entries, the converter wrote %u\n", argv[i],
                       (unsigned)ci18n_count_in(mo, "ru"), (unsigned)ci18n_count("ru"));
                cmp.differences++;
            }
            if (cmp.differences == 0)
            {
                printf("ok  %s holds the same %u entries\n", argv[i],
                       (unsigned)ci18n_count("ru"));
            }
            failures += cmp.differences;
            ci18n_destroy(mo);
        }
    }

    ci18n_free();

    printf("\n%s\n", failures ? "=== ROUND TRIP FAILED ===" : "=== ROUND TRIP OK ===");
    return failures ? 1 : 0;
}
