/*
 * Unit tests for ci18n library
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void name(void)
#define RUN_TEST(name)                   \
    do                                   \
    {                                    \
        tests_run++;                     \
        printf("Running %s... ", #name); \
        name();                          \
        tests_passed++;                  \
        printf("PASSED\n");              \
    } while (0)

#define ASSERT(cond)                                    \
    do                                                  \
    {                                                   \
        if (!(cond))                                    \
        {                                               \
            printf("FAILED\n");                         \
            printf("  Assertion failed: %s\n", #cond);  \
            printf("  at %s:%d\n", __FILE__, __LINE__); \
            tests_failed++;                             \
            return;                                     \
        }                                               \
    } while (0)

#define ASSERT_STR_EQ(a, b)                                     \
    do                                                          \
    {                                                           \
        const char *_a = (a);                                   \
        const char *_b = (b);                                   \
        if (_a == NULL || _b == NULL)                           \
        {                                                       \
            printf("FAILED\n");                                 \
            printf("  Expected: \"%s\"\n", _b ? _b : "(null)"); \
            printf("  Got:      \"%s\"\n", _a ? _a : "(null)"); \
            tests_failed++;                                     \
            return;                                             \
        }                                                       \
        if (strcmp(_a, _b) != 0)                                \
        {                                                       \
            printf("FAILED\n");                                 \
            printf("  Expected: \"%s\"\n", _b);                 \
            printf("  Got:      \"%s\"\n", _a);                 \
            tests_failed++;                                     \
            return;                                             \
        }                                                       \
    } while (0)

/* ============================================================================
 * Test Cases
 * ============================================================================ */

TEST(test_init_free)
{
    ASSERT(ci18n_init() == true);
    ASSERT(ci18n_is_initialized() == true);
    ci18n_free();
    ASSERT(ci18n_is_initialized() == false);
}

TEST(test_init_twice)
{
    ASSERT(ci18n_init() == true);
    ASSERT(ci18n_init() == true); /* Should be idempotent */
    ci18n_free();
}

TEST(test_load_from_buffer)
{
    ci18n_init();

    const char *buffer =
        "greeting=Hello\n"
        "farewell=Goodbye\n"
        "# comment\n"
        "welcome=Welcome!\n";

    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);
    ASSERT(ci18n_count("en") == 3);

    ci18n_free();
}

TEST(test_load_from_buffer_empty)
{
    ci18n_init();

    ASSERT(ci18n_load_from_buffer("en", "", 0) == true);
    ASSERT(ci18n_count("en") == 0);

    ci18n_free();
}

TEST(test_load_from_buffer_with_empty_lines)
{
    ci18n_init();

    const char *buffer =
        "key1=value1\n"
        "\n"
        "key2=value2\n"
        "\n";

    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_free();
}

TEST(test_set_get)
{
    ci18n_init();

    ASSERT(ci18n_set("en", "key1", "value1") == true);
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("key1"), "value1");

    ASSERT(ci18n_set("en", "key2", "value2") == true);
    ASSERT_STR_EQ(ci18n_get("key2"), "value2");

    ci18n_free();
}

TEST(test_set_overwrite)
{
    ci18n_init();

    ci18n_set("en", "key", "original");
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("key"), "original");

    ci18n_set("en", "key", "updated");
    ASSERT_STR_EQ(ci18n_get("key"), "updated");
    ASSERT(ci18n_count("en") == 1); /* Should not create duplicate */

    ci18n_free();
}

TEST(test_get_nonexistent)
{
    ci18n_init();

    ci18n_set("en", "existing", "value");

    ASSERT(ci18n_get("nonexistent") == NULL);

    ci18n_free();
}

TEST(test_get_or_key)
{
    ci18n_init();

    ci18n_set("en", "existing", "value");
    ci18n_set_current("en");

    ASSERT_STR_EQ(ci18n_get_or_key("existing"), "value");
    ASSERT_STR_EQ(ci18n_get_or_key("nonexistent"), "nonexistent");

    ci18n_free();
}

TEST(test_has)
{
    ci18n_init();

    ci18n_set("en", "key", "value");
    ci18n_set_current("en");

    ASSERT(ci18n_has("key") == true);
    ASSERT(ci18n_has("nonexistent") == false);

    ci18n_free();
}

TEST(test_set_current)
{
    ci18n_init();

    ci18n_set("en", "key", "english");
    ci18n_set("ru", "key", "russian");

    ASSERT(ci18n_set_current("en") == true);
    ASSERT_STR_EQ(ci18n_get("key"), "english");

    ASSERT(ci18n_set_current("ru") == true);
    ASSERT_STR_EQ(ci18n_get("key"), "russian");

    ASSERT(ci18n_set_current("nonexistent") == false);

    ci18n_free();
}

TEST(test_set_fallback)
{
    ci18n_init();

    ci18n_set("en", "key", "english");
    ci18n_set("ru", "key", "russian");
    ci18n_set("ru", "only_ru", "russian_only");

    ci18n_set_current("ru");
    ci18n_set_fallback("en");

    /* Should get Russian value */
    ASSERT_STR_EQ(ci18n_get("key"), "russian");

    /* Should fallback to English for missing key */
    ci18n_set("en", "fallback_key", "fallback_value");
    ASSERT_STR_EQ(ci18n_get("fallback_key"), "fallback_value");

    ci18n_free();
}

TEST(test_remove)
{
    ci18n_init();

    ci18n_set("en", "key1", "value1");
    ci18n_set("en", "key2", "value2");
    ci18n_set_current("en");

    ASSERT(ci18n_count("en") == 2);

    ASSERT(ci18n_remove("en", "key1") == true);
    ASSERT(ci18n_count("en") == 1);
    ASSERT(ci18n_get("key1") == NULL);
    ASSERT_STR_EQ(ci18n_get("key2"), "value2");

    ASSERT(ci18n_remove("en", "nonexistent") == false);

    ci18n_free();
}

TEST(test_clear)
{
    ci18n_init();

    ci18n_set("en", "key1", "value1");
    ci18n_set("en", "key2", "value2");
    ci18n_set("ru", "key", "russian");

    ASSERT(ci18n_clear("en") == true);
    ASSERT(ci18n_count("en") == 0);
    ASSERT(ci18n_count("ru") == 1); /* Other language unaffected */

    ASSERT(ci18n_clear("nonexistent") == false);

    ci18n_free();
}

TEST(test_clear_resets_current)
{
    ci18n_init();

    ci18n_set("en", "key", "value");
    ci18n_set_current("en");

    ASSERT_STR_EQ(ci18n_get_current(), "en");

    ci18n_clear("en");

    ASSERT_STR_EQ(ci18n_get_current(), ""); /* Should be reset */

    ci18n_free();
}

TEST(test_get_languages)
{
    ci18n_init();

    ci18n_set("en", "k", "v");
    ci18n_set("ru", "k", "v");
    ci18n_set("es", "k", "v");

    size_t count;
    const char **langs = ci18n_get_languages(&count);

    ASSERT(count == 3);

    /* Check all languages are present (order may vary) */
    int found_en = 0, found_ru = 0, found_es = 0;
    for (size_t i = 0; i < count; i++)
    {
        if (strcmp(langs[i], "en") == 0)
            found_en = 1;
        if (strcmp(langs[i], "ru") == 0)
            found_ru = 1;
        if (strcmp(langs[i], "es") == 0)
            found_es = 1;
    }

    ASSERT(found_en && found_ru && found_es);

    ci18n_free();
}

TEST(test_utf8_strings)
{
    ci18n_init();

    /* Russian */
    ci18n_set("ru", "greeting", "Привет!");
    ci18n_set_current("ru");
    ASSERT_STR_EQ(ci18n_get("greeting"), "Привет!");

    /* Spanish */
    ci18n_set("es", "greeting", "¡Hola!");
    ci18n_set_current("es");
    ASSERT_STR_EQ(ci18n_get("greeting"), "¡Hola!");

    /* Chinese */
    ci18n_set("zh", "greeting", "你好!");
    ci18n_set_current("zh");
    ASSERT_STR_EQ(ci18n_get("greeting"), "你好!");

    /* Emoji */
    ci18n_set("en", "emoji", "Hello 👋 World 🌍");
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("emoji"), "Hello 👋 World 🌍");

    ci18n_free();
}

TEST(test_long_value)
{
    ci18n_init();

    const char *long_value =
        "This is a very long translation value that exceeds typical "
        "lengths. It should still work correctly as long as it's within "
        "CI18N_MAX_VALUE_LENGTH (default 4096 bytes). Testing with this "
        "longer string to ensure buffer handling works properly.";

    ci18n_set("en", "long_key", long_value);
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("long_key"), long_value);

    ci18n_free();
}

TEST(test_empty_key_rejected)
{
    ci18n_init();

    const char *buffer = "=empty_key_value\n";
    ci18n_load_from_buffer("en", buffer, strlen(buffer));
    ASSERT(ci18n_count("en") == 0); /* Empty key should be rejected */

    ci18n_free();
}

TEST(test_whitespace_trimming)
{
    ci18n_init();

    const char *buffer =
        "  key1  =  value1  \n"
        "\tkey2\t=\tvalue2\t\n";

    ci18n_load_from_buffer("en", buffer, strlen(buffer));
    ci18n_set_current("en");

    ASSERT_STR_EQ(ci18n_get("key1"), "value1");
    ASSERT_STR_EQ(ci18n_get("key2"), "value2");

    ci18n_free();
}

TEST(test_multiple_equals_in_value)
{
    ci18n_init();

    const char *buffer = "equation=a=b+c\n";
    ci18n_load_from_buffer("en", buffer, strlen(buffer));
    ci18n_set_current("en");

    ASSERT_STR_EQ(ci18n_get("equation"), "a=b+c");

    ci18n_free();
}

TEST(test_get_current)
{
    ci18n_init();

    ASSERT_STR_EQ(ci18n_get_current(), ""); /* Empty before set */

    ci18n_set("en", "k", "v");
    ci18n_set_current("en");

    ASSERT_STR_EQ(ci18n_get_current(), "en");

    ci18n_free();
}

TEST(test_count_nonexistent_language)
{
    ci18n_init();

    ASSERT(ci18n_count("nonexistent") == 0);

    ci18n_free();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    printf("=== ci18n Unit Tests ===\n\n");

    RUN_TEST(test_init_free);
    RUN_TEST(test_init_twice);
    RUN_TEST(test_load_from_buffer);
    RUN_TEST(test_load_from_buffer_empty);
    RUN_TEST(test_load_from_buffer_with_empty_lines);
    RUN_TEST(test_set_get);
    RUN_TEST(test_set_overwrite);
    RUN_TEST(test_get_nonexistent);
    RUN_TEST(test_get_or_key);
    RUN_TEST(test_has);
    RUN_TEST(test_set_current);
    RUN_TEST(test_set_fallback);
    RUN_TEST(test_remove);
    RUN_TEST(test_clear);
    RUN_TEST(test_clear_resets_current);
    RUN_TEST(test_get_languages);
    RUN_TEST(test_utf8_strings);
    RUN_TEST(test_long_value);
    RUN_TEST(test_empty_key_rejected);
    RUN_TEST(test_whitespace_trimming);
    RUN_TEST(test_multiple_equals_in_value);
    RUN_TEST(test_get_current);
    RUN_TEST(test_count_nonexistent_language);

    printf("\n=== Results ===\n");
    printf("Total:   %d\n", tests_run);
    printf("Passed:  %d\n", tests_passed);
    printf("Failed:  %d\n", tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
