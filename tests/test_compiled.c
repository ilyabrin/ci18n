/*
 * Tests for compiled catalogues.
 *
 *   make test-compiled
 *
 * The heart of it is equivalence: every file here is loaded at run time and
 * also compiled by tools/ci18n_compile, and the two have to answer every
 * question the same way, byte for byte. Two routes to the same data are only
 * safe while something checks that they agree. The rest covers what only
 * compiled languages do: refusing writes, mixing with loaded ones, and
 * checked keys.
 *
 * The make target generates the headers into build/compiled/ first.
 *
 * SPDX-License-Identifier: MIT
 */

#define _CRT_SECURE_NO_WARNINGS
#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include "stress.h"
#include "tr_en.h"
#include "tr_ru.h"
#include "tr_es.h"
#include "sync_ru.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                             \
    do                                                                          \
    {                                                                           \
        checks++;                                                               \
        if (!(cond))                                                            \
        {                                                                       \
            printf("  FAILED: %s\n  at %s:%d\n", #cond, __FILE__, __LINE__);    \
            failures++;                                                         \
        }                                                                       \
    } while (0)

static bool same(const char *a, const char *b)
{
    return (a == NULL && b == NULL) || (a && b && strcmp(a, b) == 0);
}

/* ------------------------------------------------------------------------
 * Equivalence
 * ------------------------------------------------------------------------ */

/*
 * The entries of one language, copied out. The comparisons run after the
 * walk, not inside it: a foreach callback must not call back into the
 * library, and calling one catalogue from inside another's walk takes their
 * locks in both orders, which ThreadSanitizer rightly reports.
 */
#define MAX_ENTRIES 64

typedef struct
{
    char keys[MAX_ENTRIES][CI18N_MAX_KEY_LENGTH];
    char values[MAX_ENTRIES][256];
    size_t count;
    bool overflow;
} copied_t;

static bool copy_entry(const char *key, const char *value, void *user_data)
{
    copied_t *c = (copied_t *)user_data;

    if (c->count == MAX_ENTRIES || strlen(value) >= sizeof(c->values[0]))
    {
        c->overflow = true;
        return false;
    }
    (void)snprintf(c->keys[c->count], sizeof(c->keys[0]), "%s", key);
    (void)snprintf(c->values[c->count], sizeof(c->values[0]), "%s", value);
    c->count++;
    return true;
}

/* Every entry one side has, the other must have with the same value. */
static int compare_entries(const copied_t *mine, ci18n_t *other)
{
    int mismatches = 0;
    size_t i;

    for (i = 0; i < mine->count; i++)
    {
        const char *theirs = ci18n_get_in(other, mine->keys[i]);

        if (!same(mine->values[i], theirs))
        {
            printf("  [%s]: [%s] against [%s]\n", mine->keys[i], mine->values[i],
                   theirs ? theirs : "(missing)");
            mismatches++;
        }
    }
    return mismatches;
}

/* The key a plural or ordinal form belongs to, or the key itself. */
static void base_of(const char *key, char *out, size_t cap)
{
    size_t n = strcspn(key, "[");

    if (n >= cap)
    {
        n = cap - 1;
    }
    memcpy(out, key, n);
    out[n] = '\0';
}

/* Plural and ordinal forms for every count, filled forms, and keys that
 * are not there, for each key the loaded side has. */
static int compare_forms(const copied_t *keys, ci18n_t *loaded, ci18n_t *compiled)
{
    int mismatches = 0;
    size_t i;

    for (i = 0; i < keys->count; i++)
    {
        char base[CI18N_MAX_KEY_LENGTH];
        char missing[CI18N_MAX_KEY_LENGTH + 8];
        char a[512];
        char b[512];
        long n;

        base_of(keys->keys[i], base, sizeof(base));
        for (n = 0; n <= 200; n++)
        {
            if (!same(ci18n_plural_in(loaded, base, n), ci18n_plural_in(compiled, base, n)) ||
                !same(ci18n_ordinal_in(loaded, base, n), ci18n_ordinal_in(compiled, base, n)))
            {
                printf("  forms of [%s] differ at %ld\n", base, n);
                mismatches++;
                break;
            }
        }

        ci18n_format_plural_in(loaded, a, sizeof(a), base, 21, "name", "Anna", NULL);
        ci18n_format_plural_in(compiled, b, sizeof(b), base, 21, "name", "Anna", NULL);
        if (strcmp(a, b) != 0)
        {
            printf("  format_plural [%s]: [%s] against [%s]\n", base, a, b);
            mismatches++;
        }

        (void)snprintf(missing, sizeof(missing), "%s#missing", keys->keys[i]);
        if (ci18n_get_in(loaded, missing) || ci18n_get_in(compiled, missing))
        {
            printf("  [%s] found where it should not be\n", missing);
            mismatches++;
        }
    }
    return mismatches;
}

static void equivalent(const char *what, const char *code, const char *path,
                       const ci18n_compiled_t *compiled)
{
    ci18n_t *loaded = ci18n_create();
    ci18n_t *built = ci18n_create();
    static copied_t from_loaded;
    static copied_t from_built;
    char a[512];
    char b[512];

    printf("%s\n", what);
    CHECK(ci18n_load_language_in(loaded, code, path));
    CHECK(ci18n_use_compiled_in(built, code, compiled));
    ci18n_set_current_in(loaded, code);
    ci18n_set_current_in(built, code);

    CHECK(ci18n_count_in(loaded, code) == ci18n_count_in(built, code));

    /* Both directions, so neither side can have an entry the other lacks. */
    memset(&from_loaded, 0, sizeof(from_loaded));
    memset(&from_built, 0, sizeof(from_built));
    ci18n_foreach_in(loaded, code, copy_entry, &from_loaded);
    ci18n_foreach_in(built, code, copy_entry, &from_built);
    CHECK(!from_loaded.overflow && !from_built.overflow);
    CHECK(from_loaded.count == ci18n_count_in(loaded, code));
    CHECK(from_built.count == ci18n_count_in(built, code));
    CHECK(compare_entries(&from_loaded, built) == 0);
    CHECK(compare_entries(&from_built, loaded) == 0);

    CHECK(compare_forms(&from_loaded, loaded, built) == 0);

    ci18n_format_in(loaded, a, sizeof(a), "greeting", "name", "Anna", NULL);
    ci18n_format_in(built, b, sizeof(b), "greeting", "name", "Anna", NULL);
    CHECK(strcmp(a, b) == 0);

    ci18n_destroy(loaded);
    ci18n_destroy(built);
}

/* ------------------------------------------------------------------------
 * What only compiled languages do
 * ------------------------------------------------------------------------ */

static void read_only(void)
{
    const char *before;

    printf("read only\n");
    ci18n_init();
    CHECK(ci18n_use_compiled("en", &ci18n_compiled_stress));

    CHECK(!ci18n_set("en", "greeting", "changed"));
    CHECK(ci18n_last_error() == CI18N_ERR_READ_ONLY);
    CHECK(!ci18n_remove("en", "greeting"));
    CHECK(ci18n_last_error() == CI18N_ERR_READ_ONLY);
    CHECK(!ci18n_clear("en"));
    CHECK(ci18n_last_error() == CI18N_ERR_READ_ONLY);
    CHECK(!ci18n_load_from_buffer("en", "a=b\n", 4));
    CHECK(ci18n_last_error() == CI18N_ERR_READ_ONLY);
    CHECK(!ci18n_load_language("en", "translations/en.txt"));
    CHECK(ci18n_last_error() == CI18N_ERR_READ_ONLY);
    CHECK(strcmp(ci18n_error_string(CI18N_ERR_READ_ONLY), "unknown error") != 0);

    /* Nothing changed, and a returned string outlives everything else. */
    ci18n_set_current("en");
    before = ci18n_get("greeting");
    CHECK(same(before, "Hello, {name}!"));
    ci18n_load_language("ru", "translations/ru.txt");
    ci18n_set("de", "x", "y");
    CHECK(ci18n_get("greeting") == before);

    /* Unloading is allowed, and leaves the slot free. */
    CHECK(ci18n_remove_language("en"));
    CHECK(!ci18n_has("greeting"));
    CHECK(ci18n_load_from_buffer("en", "a=b\n", 4));
    CHECK(ci18n_count("en") == 1);
    ci18n_free();
}

static void replace_and_mix(void)
{
    printf("replacing and mixing\n");
    ci18n_init();

    /* Handing over a compiled language replaces what was loaded. */
    ci18n_load_from_buffer("ru", "only_loaded=1\n", 14);
    CHECK(ci18n_use_compiled("ru", &ci18n_compiled_tr_ru));
    CHECK(ci18n_count("ru") == ci18n_compiled_tr_ru.count);
    ci18n_set_current("ru");
    CHECK(!ci18n_has("only_loaded"));

    /* Compiled current, loaded fallback. */
    ci18n_load_from_buffer("en", "only_in_en=from the fallback\n", 29);
    ci18n_set_fallback("en");
    CHECK(same(ci18n_get("only_in_en"), "from the fallback"));

    /* Loaded current, compiled fallback, with the fallback's own plurals. */
    ci18n_load_from_buffer("de", "hallo=Hallo\n", 12);
    CHECK(ci18n_use_compiled("en", &ci18n_compiled_tr_en));
    ci18n_set_current("de");
    ci18n_set_fallback("en");
    CHECK(same(ci18n_get("hallo"), "Hallo"));
    CHECK(same(ci18n_plural("files", 1), "%d file"));
    CHECK(same(ci18n_plural("files", 21), "%d files"));

    CHECK(ci18n_get_languages(NULL, 0) == 3);
    ci18n_free();
}

static void refusals(void)
{
    ci18n_compiled_t future = ci18n_compiled_stress;

    printf("refusals\n");
    CHECK(!ci18n_use_compiled("en", &ci18n_compiled_stress));
    CHECK(ci18n_last_error() == CI18N_ERR_NOT_INITIALIZED);

    ci18n_init();
    CHECK(!ci18n_use_compiled("en", NULL));
    CHECK(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);
    CHECK(!ci18n_use_compiled(NULL, &ci18n_compiled_stress));
    CHECK(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    /* Data from a generator of another format version is not read. */
    future.format = CI18N_COMPILED_FORMAT + 1;
    CHECK(!ci18n_use_compiled("en", &future));
    CHECK(ci18n_last_error() == CI18N_ERR_PARSE);
    CHECK(ci18n_get_languages(NULL, 0) == 0);
    ci18n_free();
}

static void checked_keys(void)
{
    printf("checked keys\n");
    ci18n_init();
    ci18n_use_compiled("en", &ci18n_compiled_stress);
    ci18n_set_current("en");

    /* The macro is the string, so it works wherever a key does. */
    CHECK(strcmp(CI18N_KEY(greeting), "greeting") == 0);
    CHECK(same(ci18n_get(CI18N_KEY(greeting)), "Hello, {name}!"));
    CHECK(same(ci18n_plural(CI18N_KEY(files), 2), "{count} files"));
    CHECK(same(ci18n_get(CI18N_KEY(empty)), ""));
    ci18n_free();
}

int main(void)
{
    ci18n_init();
    ci18n_free();

    equivalent("stress.txt", "en", "tests/compiled/stress.txt", &ci18n_compiled_stress);
    equivalent("translations/en.txt", "en", "translations/en.txt", &ci18n_compiled_tr_en);
    equivalent("translations/ru.txt", "ru", "translations/ru.txt", &ci18n_compiled_tr_ru);
    equivalent("translations/es.txt", "es", "translations/es.txt", &ci18n_compiled_tr_es);
    equivalent("examples/cli_sync/locales/ru.txt", "ru", "examples/cli_sync/locales/ru.txt",
               &ci18n_compiled_sync_ru);

    /* One file under the rules of every language, so every plural and
     * ordinal family is walked from both sides. */
    {
        static const char *const codes[] = {"ar", "cy", "fr", "ga", "he", "lt", "lv", "pl",
                                            "ro", "ru", "sl", "cs", "it", "ja", "ka", "kk"};
        size_t i;

        for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
        {
            equivalent(codes[i], codes[i], "tests/compiled/stress.txt", &ci18n_compiled_stress);
        }
    }

    read_only();
    replace_and_mix();
    refusals();
    checked_keys();

    printf("\n%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
