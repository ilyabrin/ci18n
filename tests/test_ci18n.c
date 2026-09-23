/*
 * Unit tests for ci18n.
 *
 *   make test
 *
 * Exits non-zero if any test fails.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

/* Set by a failing assertion, read by RUN_TEST. A test body cannot report its
 * own result: the assertion macros return early, so only the runner knows
 * whether the body ran to completion. */
static int test_failed;

#define TEST(name) static void name(void)

#define RUN_TEST(name)                   \
    do                                   \
    {                                    \
        tests_run++;                     \
        test_failed = 0;                 \
        printf("Running %s... ", #name); \
        name();                          \
        if (test_failed)                 \
        {                                \
            tests_failed++;              \
        }                                \
        else                             \
        {                                \
            tests_passed++;              \
            printf("PASSED\n");          \
        }                                \
    } while (0)

/* Reports the failure and leaves the test body. Every assertion prints the
 * trailing newline the "Running ..." line is still missing. */
#define FAIL(...)            \
    do                       \
    {                        \
        printf("FAILED\n");  \
        printf(__VA_ARGS__); \
        test_failed = 1;     \
        return;              \
    } while (0)

#define ASSERT(cond)                                                       \
    do                                                                     \
    {                                                                      \
        if (!(cond))                                                       \
        {                                                                  \
            FAIL("  Assertion failed: %s\n  at %s:%d\n",                   \
                 #cond, __FILE__, __LINE__);                               \
        }                                                                  \
    } while (0)

#define ASSERT_STR_EQ(a, b)                                                \
    do                                                                     \
    {                                                                      \
        const char *_a = (a);                                              \
        const char *_b = (b);                                              \
        if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0)               \
        {                                                                  \
            FAIL("  Expected: \"%s\"\n  Got:      \"%s\"\n  at %s:%d\n",   \
                 _b ? _b : "(null)", _a ? _a : "(null)",                   \
                 __FILE__, __LINE__);                                      \
        }                                                                  \
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

    const char *codes[CI18N_MAX_LANGUAGES];
    size_t total = ci18n_get_languages(codes, CI18N_MAX_LANGUAGES);

    ASSERT(total == 3);

    /* Check all languages are present (order may vary) */
    int found_en = 0, found_ru = 0, found_es = 0;
    for (size_t i = 0; i < total; i++)
    {
        if (strcmp(codes[i], "en") == 0)
            found_en = 1;
        if (strcmp(codes[i], "ru") == 0)
            found_ru = 1;
        if (strcmp(codes[i], "es") == 0)
            found_es = 1;
    }

    ASSERT(found_en && found_ru && found_es);

    ci18n_free();
}

TEST(test_get_languages_count_only)
{
    ci18n_init();

    ci18n_set("en", "k", "v");
    ci18n_set("ru", "k", "v");

    /* A NULL buffer asks for the total without writing anything. */
    ASSERT(ci18n_get_languages(NULL, 0) == 2);

    ci18n_free();
}

TEST(test_get_languages_small_buffer)
{
    ci18n_init();

    ci18n_set("en", "k", "v");
    ci18n_set("ru", "k", "v");
    ci18n_set("es", "k", "v");

    /* A short buffer is filled to capacity, never past it, and the return
     * value still reports the real total. The guard entry must survive. */
    const char *codes[3];
    codes[2] = "guard";

    ASSERT(ci18n_get_languages(codes, 2) == 3);
    ASSERT_STR_EQ(codes[2], "guard");
    ASSERT(codes[0] != NULL && codes[1] != NULL);

    ci18n_free();
}

TEST(test_get_languages_before_init)
{
    /* No context yet, so nothing to report and nothing written. */
    ASSERT(ci18n_get_languages(NULL, 0) == 0);
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
 * File loading
 *
 * ci18n_load_language() is the only entry point that touches the filesystem,
 * and it is the one most likely to be handed a file the program did not
 * write. Fixtures are written in binary mode so a test controls the exact
 * line terminators, then removed.
 * ============================================================================ */

#define TEMP_FILE "ci18n_test_tmp.txt"

static int write_file(const char *path, const char *bytes, size_t len)
{
    FILE *f = fopen(path, "wb");
    size_t written = 0;

    if (!f)
    {
        return 0;
    }

    if (len > 0)
    {
        written = fwrite(bytes, 1, len, f);
    }

    fclose(f);
    return written == len;
}

/* Writes a NUL-terminated fixture. */
static int write_text(const char *path, const char *text)
{
    return write_file(path, text, strlen(text));
}

TEST(test_load_language_from_file)
{
    ci18n_init();

    ASSERT(write_text(TEMP_FILE, "greeting=Hello\nfarewell=Goodbye\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("greeting"), "Hello");
    ASSERT_STR_EQ(ci18n_get("farewell"), "Goodbye");

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_missing_file)
{
    ci18n_init();

    ASSERT(ci18n_load_language("en", "no_such_file_here.txt") == false);

    /* A failed open must not leave a half-created language behind. */
    ASSERT(ci18n_get_languages(NULL, 0) == 0);

    ci18n_free();
}

TEST(test_load_language_empty_file)
{
    ci18n_init();

    ASSERT(write_file(TEMP_FILE, "", 0));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 0);

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_crlf)
{
    ci18n_init();

    /* A file authored on Windows. The value must not keep the CR. */
    ASSERT(write_text(TEMP_FILE, "a=one\r\nb=two\r\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("a"), "one");
    ASSERT_STR_EQ(ci18n_get("b"), "two");

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_lone_cr)
{
    ci18n_init();

    /* Classic Mac terminators. fgets() does not split on CR, so the whole
     * file arrives as one line and only the first pair survives. Asserted
     * as the behaviour it is, not as the behaviour one might want. */
    ASSERT(write_text(TEMP_FILE, "a=one\rb=two\r"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 1);

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_no_trailing_newline)
{
    ci18n_init();

    ASSERT(write_text(TEMP_FILE, "a=one\nb=two"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("b"), "two");

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_bom)
{
    ci18n_init();

    /* A UTF-8 BOM used to make the first key unreachable. */
    ASSERT(write_text(TEMP_FILE, "\xEF\xBB\xBFgreeting=Hello\nfarewell=Goodbye\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("greeting"), "Hello");

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_comments_and_blank_lines)
{
    ci18n_init();

    ASSERT(write_text(TEMP_FILE,
                      "# a hash comment\n"
                      "; a semicolon comment\n"
                      "\n"
                      "   \n"
                      "  a=one\n"
                      "\t# indented comment\n"
                      "b=two\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("a"), "one");

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_malformed_lines_are_skipped)
{
    ci18n_init();

    /* Lines with no separator are dropped and the rest still load. Note that
     * the call reports success even when every line is malformed: there are
     * no parse diagnostics yet. */
    ASSERT(write_text(TEMP_FILE,
                      "this line has no separator\n"
                      "a=one\n"
                      "=value without a key\n"
                      "   =also no key\n"
                      "b=two\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 2);

    ASSERT(write_text(TEMP_FILE, "nothing here is valid\nnor here\n"));
    ASSERT(ci18n_load_language("garbage", TEMP_FILE) == true);
    ASSERT(ci18n_count("garbage") == 0);

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_merges_into_existing)
{
    ci18n_init();

    ASSERT(write_text(TEMP_FILE, "a=one\nb=two\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);

    /* A second load adds new keys and overwrites the ones it repeats. */
    ASSERT(write_text(TEMP_FILE, "b=TWO\nc=three\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 3);
    ASSERT(ci18n_get_languages(NULL, 0) == 1);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("a"), "one");
    ASSERT_STR_EQ(ci18n_get("b"), "TWO");
    ASSERT_STR_EQ(ci18n_get("c"), "three");

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_language_utf8_from_file)
{
    ci18n_init();

    ASSERT(write_text(TEMP_FILE, "greeting=\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82\n"));
    ASSERT(ci18n_load_language("ru", TEMP_FILE) == true);

    ci18n_set_current("ru");
    ASSERT_STR_EQ(ci18n_get("greeting"), "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82");

    remove(TEMP_FILE);
    ci18n_free();
}

/* ============================================================================
 * Argument handling and limits
 * ============================================================================ */

TEST(test_null_arguments)
{
    ci18n_init();

    ci18n_set("en", "k", "v");
    ci18n_set_current("en");

    ASSERT(ci18n_load_language(NULL, "f.txt") == false);
    ASSERT(ci18n_load_language("en", NULL) == false);
    ASSERT(ci18n_load_from_buffer(NULL, "a=b", 3) == false);
    ASSERT(ci18n_load_from_buffer("en", NULL, 3) == false);

    ASSERT(ci18n_set_current(NULL) == false);
    ASSERT(ci18n_set_fallback(NULL) == false);

    ASSERT(ci18n_get(NULL) == NULL);
    ASSERT(ci18n_get_or_key(NULL) == NULL);
    ASSERT(ci18n_has(NULL) == false);

    ASSERT(ci18n_set(NULL, "k", "v") == false);
    ASSERT(ci18n_set("en", NULL, "v") == false);
    ASSERT(ci18n_set("en", "k", NULL) == false);

    ASSERT(ci18n_remove(NULL, "k") == false);
    ASSERT(ci18n_remove("en", NULL) == false);
    ASSERT(ci18n_clear(NULL) == false);
    ASSERT(ci18n_count(NULL) == 0);

    /* None of the above may have disturbed the real entry. */
    ASSERT_STR_EQ(ci18n_get("k"), "v");
    ASSERT(ci18n_count("en") == 1);

    ci18n_free();
}

TEST(test_calls_before_init)
{
    /* Nothing may touch the context before ci18n_init(). Reached here with
     * no init because the previous test freed. */
    ASSERT(ci18n_is_initialized() == false);

    ASSERT(ci18n_load_language("en", "f.txt") == false);
    ASSERT(ci18n_load_from_buffer("en", "a=b", 3) == false);
    ASSERT(ci18n_set("en", "k", "v") == false);
    ASSERT(ci18n_set_current("en") == false);
    ASSERT(ci18n_set_fallback("en") == false);
    ASSERT(ci18n_get("k") == NULL);
    ASSERT(ci18n_has("k") == false);
    ASSERT(ci18n_remove("en", "k") == false);
    ASSERT(ci18n_clear("en") == false);
    ASSERT(ci18n_count("en") == 0);
    ASSERT(ci18n_get_languages(NULL, 0) == 0);
    ASSERT_STR_EQ(ci18n_get_current(), "");

    /* ci18n_free() on an uninitialized context must be a no-op, not a crash. */
    ci18n_free();
}

TEST(test_max_languages)
{
    char code[16];
    size_t i;

    ci18n_init();

    for (i = 0; i < CI18N_MAX_LANGUAGES; i++)
    {
        snprintf(code, sizeof(code), "l%u", (unsigned int)i);
        ASSERT(ci18n_set(code, "k", "v") == true);
    }

    ASSERT(ci18n_get_languages(NULL, 0) == CI18N_MAX_LANGUAGES);

    /* One past the limit fails instead of overflowing the array. */
    ASSERT(ci18n_set("overflow", "k", "v") == false);
    ASSERT(ci18n_get_languages(NULL, 0) == CI18N_MAX_LANGUAGES);

    /* An existing language still works while the table is full. */
    ASSERT(ci18n_set("l0", "another", "v") == true);

    ci18n_free();
}

TEST(test_max_keys_per_language)
{
    char key[16];
    size_t i;

    ci18n_init();

    for (i = 0; i < CI18N_MAX_KEYS_PER_LANGUAGE; i++)
    {
        snprintf(key, sizeof(key), "k%u", (unsigned int)i);
        ASSERT(ci18n_set("en", key, "v") == true);
    }

    ASSERT(ci18n_count("en") == CI18N_MAX_KEYS_PER_LANGUAGE);

    ASSERT(ci18n_set("en", "overflow", "v") == false);
    ASSERT(ci18n_count("en") == CI18N_MAX_KEYS_PER_LANGUAGE);

    /* Overwriting an existing key needs no new slot, so it still works. */
    ASSERT(ci18n_set("en", "k0", "updated") == true);
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("k0"), "updated");

    ci18n_free();
}

TEST(test_long_key_is_truncated)
{
    char long_key[CI18N_MAX_KEY_LENGTH + 64];
    char truncated[CI18N_MAX_KEY_LENGTH];

    ci18n_init();

    memset(long_key, 'k', sizeof(long_key) - 1);
    long_key[sizeof(long_key) - 1] = '\0';

    memcpy(truncated, long_key, CI18N_MAX_KEY_LENGTH - 1);
    truncated[CI18N_MAX_KEY_LENGTH - 1] = '\0';

    /* Stored under the truncated key, silently. The full key then misses,
     * which is why over-long keys are a documented sharp edge. */
    ASSERT(ci18n_set("en", long_key, "v") == true);
    ci18n_set_current("en");

    ASSERT(ci18n_get(long_key) == NULL);
    ASSERT_STR_EQ(ci18n_get(truncated), "v");

    ci18n_free();
}

TEST(test_long_value_is_truncated)
{
    char long_value[CI18N_MAX_VALUE_LENGTH + 64];
    const char *stored;

    ci18n_init();

    memset(long_value, 'v', sizeof(long_value) - 1);
    long_value[sizeof(long_value) - 1] = '\0';

    ASSERT(ci18n_set("en", "k", long_value) == true);
    ci18n_set_current("en");

    stored = ci18n_get("k");
    ASSERT(stored != NULL);
    ASSERT(strlen(stored) == CI18N_MAX_VALUE_LENGTH - 1);

    ci18n_free();
}

TEST(test_language_code_at_the_limit)
{
    char code[CI18N_MAX_CODE_LENGTH];

    ci18n_init();

    /* The longest code that still fits, terminator included. */
    memset(code, 'c', sizeof(code) - 1);
    code[sizeof(code) - 1] = '\0';

    ASSERT(ci18n_set(code, "k", "v") == true);
    ASSERT(ci18n_set_current(code) == true);
    ASSERT_STR_EQ(ci18n_get("k"), "v");
    ASSERT(ci18n_count(code) == 1);

    ci18n_free();
}

TEST(test_long_language_code_is_rejected)
{
    char too_long[CI18N_MAX_CODE_LENGTH + 8];

    ci18n_init();

    memset(too_long, 'c', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';

    /* Truncating used to be quietly destructive: the shortened code was
     * stored while lookups compared the full string, so the value could be
     * written and never read, and a second code with the same prefix created
     * a duplicate, permanently unreachable language. Now it just fails. */
    ASSERT(ci18n_set(too_long, "k", "v") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_CODE_TOO_LONG);
    ASSERT(ci18n_get_languages(NULL, 0) == 0);

    ASSERT(ci18n_load_language(too_long, "whatever.txt") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_CODE_TOO_LONG);

    ASSERT(ci18n_load_from_buffer(too_long, "a=b", 3) == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_CODE_TOO_LONG);

    ci18n_free();
}

/* ============================================================================
 * Storage layer
 *
 * Keys and values live in a per-language arena and are found through a hash
 * table. These cover the paths that has no equivalent in a flat array: chain
 * walking, removal by swapping the last entry into the hole, rewinding the
 * arena, and updating a value that no longer fits where the old one sat.
 * ============================================================================ */

TEST(test_many_keys_all_reachable)
{
    char key[32];
    char value[32];
    int i;

    ci18n_init();

    /* Enough entries to force several bucket growths and real chains. */
    for (i = 0; i < 500; i++)
    {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        ASSERT(ci18n_set("en", key, value) == true);
    }

    ASSERT(ci18n_count("en") == 500);
    ci18n_set_current("en");

    /* Every one of them, not just the last: a rehash that dropped an entry
     * would leave the count right and the lookup wrong. */
    for (i = 0; i < 500; i++)
    {
        snprintf(key, sizeof(key), "key_%d", i);
        snprintf(value, sizeof(value), "value_%d", i);
        ASSERT_STR_EQ(ci18n_get(key), value);
    }

    /* Keys that were never added must still miss. */
    ASSERT(ci18n_get("key_500") == NULL);
    ASSERT(ci18n_get("key_") == NULL);
    ASSERT(ci18n_get("") == NULL);

    ci18n_free();
}

TEST(test_remove_keeps_the_rest_reachable)
{
    char key[32];
    int i;

    ci18n_init();

    for (i = 0; i < 100; i++)
    {
        snprintf(key, sizeof(key), "key_%d", i);
        ci18n_set("en", key, "v");
    }

    ci18n_set_current("en");

    /* Remove every other key. Removal moves the last entry into the hole, so
     * the bucket chains have to be rebuilt around it each time. */
    for (i = 0; i < 100; i += 2)
    {
        snprintf(key, sizeof(key), "key_%d", i);
        ASSERT(ci18n_remove("en", key) == true);
    }

    ASSERT(ci18n_count("en") == 50);

    for (i = 0; i < 100; i++)
    {
        snprintf(key, sizeof(key), "key_%d", i);

        if (i % 2 == 0)
        {
            ASSERT(ci18n_get(key) == NULL);
        }
        else
        {
            ASSERT(ci18n_get(key) != NULL);
        }
    }

    /* Removing the same key twice is a miss, not a corruption. */
    ASSERT(ci18n_remove("en", "key_0") == false);
    ASSERT(ci18n_count("en") == 50);

    /* And the table still accepts new entries afterwards. */
    ASSERT(ci18n_set("en", "added_after", "v") == true);
    ASSERT_STR_EQ(ci18n_get("added_after"), "v");

    ci18n_free();
}

TEST(test_value_update_shorter_and_longer)
{
    ci18n_init();
    ci18n_set_current("en");

    ASSERT(ci18n_set("en", "k", "medium length") == true);
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("k"), "medium length");

    /* Shorter fits where the old value sat and is written in place. */
    ASSERT(ci18n_set("en", "k", "short") == true);
    ASSERT_STR_EQ(ci18n_get("k"), "short");

    /* Longer cannot, so it is appended and the entry repointed. */
    ASSERT(ci18n_set("en", "k", "a considerably longer replacement value") == true);
    ASSERT_STR_EQ(ci18n_get("k"), "a considerably longer replacement value");

    /* Back to short again, and a neighbour must be untouched throughout. */
    ci18n_set("en", "neighbour", "intact");
    ASSERT(ci18n_set("en", "k", "s") == true);
    ASSERT_STR_EQ(ci18n_get("k"), "s");
    ASSERT_STR_EQ(ci18n_get("neighbour"), "intact");
    ASSERT(ci18n_count("en") == 2);

    ci18n_free();
}

TEST(test_clear_then_reuse)
{
    char key[32];
    int i;

    ci18n_init();

    for (i = 0; i < 50; i++)
    {
        snprintf(key, sizeof(key), "key_%d", i);
        ci18n_set("en", key, "first round");
    }

    ASSERT(ci18n_clear("en") == true);
    ASSERT(ci18n_count("en") == 0);

    /* Clearing rewinds the arena, so the second round writes over the first.
     * Everything has to be findable again afterwards. */
    for (i = 0; i < 50; i++)
    {
        snprintf(key, sizeof(key), "key_%d", i);
        ASSERT(ci18n_set("en", key, "second round") == true);
    }

    ASSERT(ci18n_count("en") == 50);
    ci18n_set_current("en");

    for (i = 0; i < 50; i++)
    {
        snprintf(key, sizeof(key), "key_%d", i);
        ASSERT_STR_EQ(ci18n_get(key), "second round");
    }

    ci18n_free();
}

TEST(test_reload_same_file_twice)
{
    ci18n_init();

    ASSERT(write_text(TEMP_FILE, "a=one\nb=two\nc=three\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 3);

    /* A second load of identical content updates every value in place and
     * must not duplicate a single entry. */
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_count("en") == 3);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("a"), "one");
    ASSERT_STR_EQ(ci18n_get("c"), "three");

    remove(TEMP_FILE);
    ci18n_free();
}

/* ============================================================================
 * Plurals
 *
 * The category tables below are the point of the feature, so they are checked
 * against counts where the families actually disagree, not just against 1 and
 * 2. Russian is the motivating case: 1, 2 and 5 take three different forms,
 * and 11 and 21 are where a naive rule gets it wrong.
 * ============================================================================ */

TEST(test_plural_category_english)
{
    ASSERT(ci18n_plural_category("en", 0) == CI18N_PLURAL_OTHER);
    ASSERT(ci18n_plural_category("en", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("en", 2) == CI18N_PLURAL_OTHER);
    ASSERT(ci18n_plural_category("en", 21) == CI18N_PLURAL_OTHER);

    /* An unknown language is treated as English-like. */
    ASSERT(ci18n_plural_category("xx", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("xx", 5) == CI18N_PLURAL_OTHER);
    ASSERT(ci18n_plural_category(NULL, 1) == CI18N_PLURAL_ONE);
}

TEST(test_plural_category_russian)
{
    /* one: 1, 21, 31 but not 11 */
    ASSERT(ci18n_plural_category("ru", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("ru", 21) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("ru", 101) == CI18N_PLURAL_ONE);

    /* few: 2 to 4, 22 to 24, but not 12 to 14 */
    ASSERT(ci18n_plural_category("ru", 2) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ru", 4) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ru", 23) == CI18N_PLURAL_FEW);

    /* many: 0, 5 to 20, and the teens that the other rules excluded */
    ASSERT(ci18n_plural_category("ru", 0) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ru", 5) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ru", 11) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ru", 12) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ru", 14) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ru", 100) == CI18N_PLURAL_MANY);

    /* A region subtag must not change the rule. */
    ASSERT(ci18n_plural_category("ru-RU", 2) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ru_RU.UTF-8", 2) == CI18N_PLURAL_FEW);

    /* Ukrainian and Belarusian share the family. */
    ASSERT(ci18n_plural_category("uk", 11) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("be", 3) == CI18N_PLURAL_FEW);
}

TEST(test_plural_category_other_families)
{
    /* No plural distinction at all. */
    ASSERT(ci18n_plural_category("ja", 1) == CI18N_PLURAL_OTHER);
    ASSERT(ci18n_plural_category("zh", 5) == CI18N_PLURAL_OTHER);
    ASSERT(ci18n_plural_category("ko", 0) == CI18N_PLURAL_OTHER);

    /* French counts zero as one. */
    ASSERT(ci18n_plural_category("fr", 0) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("fr", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("fr", 2) == CI18N_PLURAL_OTHER);

    /* Polish: 1 alone is one, and unlike Russian 0 is many. */
    ASSERT(ci18n_plural_category("pl", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("pl", 2) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("pl", 5) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("pl", 22) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("pl", 12) == CI18N_PLURAL_MANY);

    /* Czech has no many for integers. */
    ASSERT(ci18n_plural_category("cs", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("cs", 3) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("cs", 5) == CI18N_PLURAL_OTHER);

    /* Croatian looks like Russian but tops out at other. */
    ASSERT(ci18n_plural_category("hr", 21) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("hr", 22) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("hr", 5) == CI18N_PLURAL_OTHER);

    /* Arabic is the one that uses all six. */
    ASSERT(ci18n_plural_category("ar", 0) == CI18N_PLURAL_ZERO);
    ASSERT(ci18n_plural_category("ar", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("ar", 2) == CI18N_PLURAL_TWO);
    ASSERT(ci18n_plural_category("ar", 3) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ar", 11) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ar", 100) == CI18N_PLURAL_OTHER);

    /* Lithuanian excludes the whole teens range. */
    ASSERT(ci18n_plural_category("lt", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("lt", 11) == CI18N_PLURAL_OTHER);
    ASSERT(ci18n_plural_category("lt", 2) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("lt", 19) == CI18N_PLURAL_OTHER);

    /* Latvian has a zero category that 0 is not the only member of. */
    ASSERT(ci18n_plural_category("lv", 0) == CI18N_PLURAL_ZERO);
    ASSERT(ci18n_plural_category("lv", 11) == CI18N_PLURAL_ZERO);
    ASSERT(ci18n_plural_category("lv", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("lv", 21) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("lv", 2) == CI18N_PLURAL_OTHER);

    /* Slovenian has a dual. */
    ASSERT(ci18n_plural_category("sl", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("sl", 2) == CI18N_PLURAL_TWO);
    ASSERT(ci18n_plural_category("sl", 3) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("sl", 5) == CI18N_PLURAL_OTHER);
    ASSERT(ci18n_plural_category("sl", 101) == CI18N_PLURAL_ONE);

    /* Irish uses four of them. */
    ASSERT(ci18n_plural_category("ga", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("ga", 2) == CI18N_PLURAL_TWO);
    ASSERT(ci18n_plural_category("ga", 5) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ga", 8) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ga", 11) == CI18N_PLURAL_OTHER);

    /* Romanian puts 0 and the teens together. */
    ASSERT(ci18n_plural_category("ro", 1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("ro", 0) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ro", 19) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ro", 20) == CI18N_PLURAL_OTHER);
}

TEST(test_plural_negative_counts)
{
    /* Minus three things is still three things. */
    ASSERT(ci18n_plural_category("ru", -1) == CI18N_PLURAL_ONE);
    ASSERT(ci18n_plural_category("ru", -2) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("ru", -5) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("en", -1) == CI18N_PLURAL_ONE);
}

TEST(test_plural_category_names)
{
    ASSERT_STR_EQ(ci18n_plural_category_name(CI18N_PLURAL_ZERO), "zero");
    ASSERT_STR_EQ(ci18n_plural_category_name(CI18N_PLURAL_ONE), "one");
    ASSERT_STR_EQ(ci18n_plural_category_name(CI18N_PLURAL_TWO), "two");
    ASSERT_STR_EQ(ci18n_plural_category_name(CI18N_PLURAL_FEW), "few");
    ASSERT_STR_EQ(ci18n_plural_category_name(CI18N_PLURAL_MANY), "many");
    ASSERT_STR_EQ(ci18n_plural_category_name(CI18N_PLURAL_OTHER), "other");
    ASSERT_STR_EQ(ci18n_plural_category_name((ci18n_plural_category_t)999), "other");
}

TEST(test_plural_category_ignores_case)
{
    /* BCP 47 subtags are case-insensitive, so an upper-case code must not
     * fall through to the English rule. */
    ASSERT(ci18n_plural_category("RU", 2) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("Ru", 5) == CI18N_PLURAL_MANY);
    ASSERT(ci18n_plural_category("ru_RU", 2) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("RU_ru.UTF-8", 3) == CI18N_PLURAL_FEW);
    ASSERT(ci18n_plural_category("JA", 1) == CI18N_PLURAL_OTHER);
}

TEST(test_direction_right_to_left_languages)
{
    ASSERT(ci18n_direction("ar") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("he") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("fa") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ur") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ps") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("sd") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ug") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("dv") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("yi") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ckb") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("prs") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("syr") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("nqo") == CI18N_DIR_RTL);

    /* The pre-1989 codes for Hebrew and Yiddish are still in circulation. */
    ASSERT(ci18n_direction("iw") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ji") == CI18N_DIR_RTL);
}

TEST(test_direction_left_to_right_languages)
{
    ASSERT(ci18n_direction("en") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("ru") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("ja") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("tr") == CI18N_DIR_LTR);

    /* Written in Latin or Gurmukhi unless a script subtag says otherwise,
     * which is exactly why they are not in the table. */
    ASSERT(ci18n_direction("ku") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("az") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("pa") == CI18N_DIR_LTR);

    /* Unknown and absent both mean left to right. */
    ASSERT(ci18n_direction("xx") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction(NULL) == CI18N_DIR_LTR);
}

TEST(test_direction_script_subtag_wins)
{
    /* A right-to-left script on a left-to-right language. */
    ASSERT(ci18n_direction("az-Arab") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("pa-Arab") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ku-Arab") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ff-Adlm") == CI18N_DIR_RTL);

    /* And the other way round: romanized Arabic reads left to right. */
    ASSERT(ci18n_direction("ar-Latn") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("fa-Latn") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("az-Cyrl") == CI18N_DIR_LTR);

    /* A script that agrees with the language changes nothing. */
    ASSERT(ci18n_direction("ar-Arab") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("he-Hebr") == CI18N_DIR_RTL);

    /* A script plus a region still works. */
    ASSERT(ci18n_direction("az-Arab-IR") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ar-Latn-EG") == CI18N_DIR_LTR);
}

TEST(test_direction_reads_only_real_script_subtags)
{
    /* A region is two letters or three digits, not four, so it must not be
     * mistaken for a script. */
    ASSERT(ci18n_direction("ar-EG") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ar-001") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ar_EG.UTF-8") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("en-US") == CI18N_DIR_LTR);

    /* "AR" here is Argentina, and Azerbaijani is still left to right. */
    ASSERT(ci18n_direction("az-AR") == CI18N_DIR_LTR);

    /* Four letters have to be the whole subtag: "Arabic" is not "Arab",
     * and neither is the "Arab" in a malformed "Arab1". */
    ASSERT(ci18n_direction("az-Arabic") == CI18N_DIR_LTR);
    ASSERT(ci18n_direction("az-Arab1") == CI18N_DIR_LTR);

    /* A variant subtag is five characters or more, so it is not a script. */
    ASSERT(ci18n_direction("de-1996") == CI18N_DIR_LTR);
}

TEST(test_direction_ignores_case)
{
    ASSERT(ci18n_direction("AR") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("Ar") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("HE-il") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("EN") == CI18N_DIR_LTR);

    /* Script subtags too, however they are cased. */
    ASSERT(ci18n_direction("az-arab") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("AZ-ARAB") == CI18N_DIR_RTL);
    ASSERT(ci18n_direction("ar-LATN") == CI18N_DIR_LTR);
}

TEST(test_direction_names)
{
    ASSERT_STR_EQ(ci18n_direction_name(CI18N_DIR_LTR), "ltr");
    ASSERT_STR_EQ(ci18n_direction_name(CI18N_DIR_RTL), "rtl");

    /* Anything else is left to right, matching the unknown-language rule. */
    ASSERT_STR_EQ(ci18n_direction_name((ci18n_direction_t)999), "ltr");
}

TEST(test_current_direction_follows_the_current_language)
{
    ci18n_init();

    /* No language set yet. */
    ASSERT(ci18n_current_direction() == CI18N_DIR_LTR);

    ci18n_set("en", "greeting", "Hello");
    ci18n_set("ar", "greeting", "مرحبا");
    ci18n_set("he", "greeting", "שלום");

    ci18n_set_current("en");
    ASSERT(ci18n_current_direction() == CI18N_DIR_LTR);
    ASSERT_STR_EQ(ci18n_direction_name(ci18n_current_direction()), "ltr");

    ci18n_set_current("ar");
    ASSERT(ci18n_current_direction() == CI18N_DIR_RTL);
    ASSERT_STR_EQ(ci18n_direction_name(ci18n_current_direction()), "rtl");

    ci18n_set_current("he");
    ASSERT(ci18n_current_direction() == CI18N_DIR_RTL);

    ci18n_free();

    /* After teardown there is no current language, so left to right again. */
    ASSERT(ci18n_current_direction() == CI18N_DIR_LTR);
}

TEST(test_current_direction_in_a_catalogue)
{
    ci18n_t *catalog = ci18n_create();

    ASSERT(catalog != NULL);
    ASSERT(ci18n_current_direction_in(catalog) == CI18N_DIR_LTR);

    ci18n_set_in(catalog, "fa", "greeting", "سلام");
    ci18n_set_current_in(catalog, "fa");
    ASSERT(ci18n_current_direction_in(catalog) == CI18N_DIR_RTL);

    /* The default catalogue is untouched by any of that. */
    ci18n_init();
    ASSERT(ci18n_current_direction() == CI18N_DIR_LTR);
    ci18n_free();

    ci18n_destroy(catalog);
}

/*
 * Byte sequences below are written as adjacent string literals, "\xC0" "\x80"
 * rather than "\xC0\x80". A hex escape in C consumes as many hex digits as it
 * can find, so the second form is one character \xC080, not two bytes, and
 * every test using it would be quietly testing something else.
 */

TEST(test_utf8_valid_accepts_well_formed)
{
    ASSERT(ci18n_utf8_valid(""));
    ASSERT(ci18n_utf8_valid("plain ascii"));
    ASSERT(ci18n_utf8_valid("Привет"));
    ASSERT(ci18n_utf8_valid("日本語"));
    ASSERT(ci18n_utf8_valid("\xF0\x9F\x91\x8D"));

    /* The first and last value of each sequence length, which is where an
     * off-by-one in the range checks would show. */
    ASSERT(ci18n_utf8_valid("\x7F"));               /* U+007F, 1 byte */
    ASSERT(ci18n_utf8_valid("\xC2" "\x80"));        /* U+0080, 2 bytes */
    ASSERT(ci18n_utf8_valid("\xDF" "\xBF"));        /* U+07FF */
    ASSERT(ci18n_utf8_valid("\xE0" "\xA0" "\x80")); /* U+0800, 3 bytes */
    ASSERT(ci18n_utf8_valid("\xED" "\x9F" "\xBF")); /* U+D7FF, just below the surrogates */
    ASSERT(ci18n_utf8_valid("\xEE" "\x80" "\x80")); /* U+E000, just above them */
    ASSERT(ci18n_utf8_valid("\xEF" "\xBF" "\xBF")); /* U+FFFF */
    ASSERT(ci18n_utf8_valid("\xF0" "\x90" "\x80" "\x80")); /* U+10000, 4 bytes */
    ASSERT(ci18n_utf8_valid("\xF4" "\x8F" "\xBF" "\xBF")); /* U+10FFFF, the last one */
}

TEST(test_utf8_valid_rejects_overlong_encodings)
{
    /* Encoding a value in more bytes than it needs. Accepting these is how a
     * check on one representation gets bypassed with another. */
    ASSERT(!ci18n_utf8_valid("\xC0" "\x80"));               /* U+0000 as 2 bytes */
    ASSERT(!ci18n_utf8_valid("\xC1" "\xBF"));               /* U+007F as 2 bytes */
    ASSERT(!ci18n_utf8_valid("\xE0" "\x80" "\x80"));        /* U+0000 as 3 */
    ASSERT(!ci18n_utf8_valid("\xE0" "\x9F" "\xBF"));        /* U+07FF as 3 */
    ASSERT(!ci18n_utf8_valid("\xF0" "\x80" "\x80" "\x80")); /* U+0000 as 4 */
    ASSERT(!ci18n_utf8_valid("\xF0" "\x8F" "\xBF" "\xBF")); /* U+FFFF as 4 */
}

TEST(test_utf8_valid_rejects_surrogates_and_out_of_range)
{
    /* U+D800 to U+DFFF exist only to pair up in UTF-16 and are not
     * characters, so they must not appear in UTF-8. */
    ASSERT(!ci18n_utf8_valid("\xED" "\xA0" "\x80")); /* U+D800 */
    ASSERT(!ci18n_utf8_valid("\xED" "\xBF" "\xBF")); /* U+DFFF */

    /* Above U+10FFFF there are no codepoints at all. */
    ASSERT(!ci18n_utf8_valid("\xF4" "\x90" "\x80" "\x80"));
    ASSERT(!ci18n_utf8_valid("\xF5" "\x80" "\x80" "\x80"));
    ASSERT(!ci18n_utf8_valid("\xFF"));
}

TEST(test_utf8_valid_rejects_malformed_sequences)
{
    /* A continuation byte with no lead byte before it. */
    ASSERT(!ci18n_utf8_valid("\x80"));
    ASSERT(!ci18n_utf8_valid("\xBF"));

    /* A sequence the string ends in the middle of. */
    ASSERT(!ci18n_utf8_valid("\xD0"));
    ASSERT(!ci18n_utf8_valid("\xE2" "\x82"));
    ASSERT(!ci18n_utf8_valid("\xF0" "\x9F" "\x91"));

    /* A byte that should continue the sequence but does not. */
    ASSERT(!ci18n_utf8_valid("\xE2" "\x28" "\xA1"));
    ASSERT(!ci18n_utf8_valid("\xD0" "z"));

    /* Valid text with one bad byte in the middle is still invalid. */
    ASSERT(!ci18n_utf8_valid("ok \x80 not ok"));

    ASSERT(!ci18n_utf8_valid(NULL));
}

TEST(test_utf8_length_counts_characters)
{
    ASSERT(ci18n_utf8_length("") == 0);
    ASSERT(ci18n_utf8_length("abc") == 3);

    /* Six characters, twelve bytes: the whole point of the function. */
    ASSERT(ci18n_utf8_length("Привет") == 6);
    ASSERT(strlen("Привет") == 12);

    ASSERT(ci18n_utf8_length("日本語") == 3);
    ASSERT(ci18n_utf8_length("\xF0\x9F\x91\x8D") == 1);
    ASSERT(ci18n_utf8_length("a\xC3\xA9\xF0\x9F\x91\x8D") == 3);

    /* An invalid byte counts as one, so the answer stays defined. */
    ASSERT(ci18n_utf8_length("\x80") == 1);
    ASSERT(ci18n_utf8_length("a\x80" "b") == 3);

    ASSERT(ci18n_utf8_length(NULL) == 0);
}

TEST(test_utf8_sequence_length_steps_one_character)
{
    ASSERT(ci18n_utf8_sequence_length("a") == 1);
    ASSERT(ci18n_utf8_sequence_length("\xC3\xA9") == 2);
    ASSERT(ci18n_utf8_sequence_length("\xE2\x82\xAC") == 3);
    ASSERT(ci18n_utf8_sequence_length("\xF0\x9F\x91\x8D") == 4);

    /* Never 0 for a non-empty string, or a stepping loop would spin. */
    ASSERT(ci18n_utf8_sequence_length("\x80") == 1);
    ASSERT(ci18n_utf8_sequence_length("\xD0") == 1);

    ASSERT(ci18n_utf8_sequence_length("") == 0);
    ASSERT(ci18n_utf8_sequence_length(NULL) == 0);

    /* Walking a whole string should land exactly on the terminator. */
    {
        const char *p = "aПривет日本語";
        size_t characters = 0;
        size_t step;

        while ((step = ci18n_utf8_sequence_length(p)) > 0)
        {
            p += step;
            characters++;
        }

        ASSERT(*p == '\0');
        ASSERT(characters == ci18n_utf8_length("aПривет日本語"));
    }
}

TEST(test_utf8_truncate_cuts_at_a_boundary)
{
    char buffer[32];

    /* "Привет" is 12 bytes of 2-byte characters, so a budget of 7 gives 6. */
    strcpy(buffer, "Привет");
    ASSERT(ci18n_utf8_truncate(buffer, 7) == 6);
    ASSERT_STR_EQ(buffer, "При");
    ASSERT(ci18n_utf8_valid(buffer));

    /* A budget that already falls on a boundary is used in full. */
    strcpy(buffer, "Привет");
    ASSERT(ci18n_utf8_truncate(buffer, 6) == 6);
    ASSERT_STR_EQ(buffer, "При");

    /* Text within budget is untouched, whether the budget is exact or ample. */
    strcpy(buffer, "Привет");
    ASSERT(ci18n_utf8_truncate(buffer, 12) == 12);
    ASSERT_STR_EQ(buffer, "Привет");

    strcpy(buffer, "Привет");
    ASSERT(ci18n_utf8_truncate(buffer, 100) == 12);
    ASSERT_STR_EQ(buffer, "Привет");

    /* Not even one character fits, so nothing is kept. */
    strcpy(buffer, "Привет");
    ASSERT(ci18n_utf8_truncate(buffer, 1) == 0);
    ASSERT_STR_EQ(buffer, "");

    strcpy(buffer, "Привет");
    ASSERT(ci18n_utf8_truncate(buffer, 0) == 0);
    ASSERT_STR_EQ(buffer, "");

    /* ASCII has no boundaries to respect, so the budget is exact. */
    strcpy(buffer, "hello");
    ASSERT(ci18n_utf8_truncate(buffer, 3) == 3);
    ASSERT_STR_EQ(buffer, "hel");

    /* A 4-byte character backs off the full three bytes. */
    strcpy(buffer, "\xF0\x9F\x91\x8D" "x");
    ASSERT(ci18n_utf8_truncate(buffer, 3) == 0);
    ASSERT_STR_EQ(buffer, "");

    strcpy(buffer, "ab\xF0\x9F\x91\x8D");
    ASSERT(ci18n_utf8_truncate(buffer, 5) == 2);
    ASSERT_STR_EQ(buffer, "ab");

    ASSERT(ci18n_utf8_truncate(NULL, 4) == 0);
}

/*
 * White-box: the decoder is also given an explicit length, so it must reject
 * a sequence running past that length. With NUL-terminated input the
 * terminator happens to reject one anyway, being no valid continuation byte,
 * so only an unterminated buffer actually exercises the length guard. Any
 * future caller decoding a mapped file or a slice of a larger buffer depends
 * on it.
 */
TEST(test_utf8_decode_respects_its_length_limit)
{
    /* Deliberately not NUL-terminated, and followed by bytes that would form
     * a valid sequence if the limit were ignored. */
    static const unsigned char two_byte_lead[] = {0xD0, 0xBF, 0xBF};
    static const unsigned char four_byte_lead[] = {0xF0, 0x9F, 0x91, 0x8D};

    /* Enough room: decoded as the 2-byte character it is. */
    ASSERT(ci18n_utf8_decode(two_byte_lead, 3) == 2);
    ASSERT(ci18n_utf8_decode(two_byte_lead, 2) == 2);

    /* One byte of room for a 2-byte character: refused without reading on. */
    ASSERT(ci18n_utf8_decode(two_byte_lead, 1) == 0);
    ASSERT(ci18n_utf8_decode(two_byte_lead, 0) == 0);

    ASSERT(ci18n_utf8_decode(four_byte_lead, 4) == 4);
    ASSERT(ci18n_utf8_decode(four_byte_lead, 3) == 0);
    ASSERT(ci18n_utf8_decode(four_byte_lead, 2) == 0);
    ASSERT(ci18n_utf8_decode(four_byte_lead, 1) == 0);
}

TEST(test_format_truncation_keeps_utf8_intact)
{
    char out[8];
    size_t needed;

    ci18n_init();
    ci18n_set("ru", "greeting", "Привет");
    ci18n_set_current("ru");

    /* 12 bytes into 8: the report is still the full length, so a caller can
     * size a buffer, but what landed is whole characters only. */
    needed = ci18n_format(out, sizeof(out), "greeting", NULL);
    ASSERT(needed == 12);
    ASSERT(strlen(out) == 6);
    ASSERT_STR_EQ(out, "При");
    ASSERT(ci18n_utf8_valid(out));

    /* The same must hold when the cut falls inside an interpolated value
     * rather than inside the literal text. */
    ci18n_set("ru", "hello", "П{name}");
    needed = ci18n_format(out, sizeof(out), "hello", "name", "риветик", NULL);
    ASSERT(needed == 16);
    ASSERT(ci18n_utf8_valid(out));
    ASSERT(strlen(out) % 2 == 0);

    ci18n_free();
}

/* ============================================================================
 * Formatters
 * ============================================================================ */

/* Uppercases ASCII. Follows snprintf, including the measuring call. */
static size_t fmt_upper(char *out, size_t capacity, const char *value,
                        const char *arg, void *user_data)
{
    size_t len = strlen(value);
    size_t i;

    (void)arg;
    (void)user_data;

    for (i = 0; i < len && capacity > 0 && i < capacity - 1; i++)
    {
        char c = value[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }

    if (capacity > 0)
    {
        out[i] = '\0';
    }

    return len;
}

/* Repeats the value as many times as the argument asks. */
static size_t fmt_repeat(char *out, size_t capacity, const char *value,
                         const char *arg, void *user_data)
{
    int times = atoi(arg);
    size_t len = strlen(value);
    size_t needed;
    size_t written = 0;
    int n;

    (void)user_data;

    if (times < 1)
    {
        times = 1;
    }
    needed = len * (size_t)times;

    for (n = 0; n < times; n++)
    {
        size_t i;
        for (i = 0; i < len && capacity > 0 && written < capacity - 1; i++)
        {
            out[written++] = value[i];
        }
    }

    if (capacity > 0)
    {
        out[written] = '\0';
    }

    return needed;
}

/* Records what it was handed, so the plumbing can be checked. */
static char seen_value[64];
static char seen_arg[64];
static void *seen_user_data;
static int seen_calls;

static size_t fmt_spy(char *out, size_t capacity, const char *value,
                      const char *arg, void *user_data)
{
    seen_calls++;
    seen_user_data = user_data;
    strncpy(seen_value, value, sizeof(seen_value) - 1);
    seen_value[sizeof(seen_value) - 1] = '\0';
    strncpy(seen_arg, arg, sizeof(seen_arg) - 1);
    seen_arg[sizeof(seen_arg) - 1] = '\0';

    if (capacity > 0)
    {
        out[0] = '\0';
    }
    return 0;
}

/* Always emits the same multi-byte text, to check truncation of its output. */
static size_t fmt_cyrillic(char *out, size_t capacity, const char *value,
                           const char *arg, void *user_data)
{
    const char *text = "Привет";
    size_t len = strlen(text);
    size_t copy = len;

    (void)value;
    (void)arg;
    (void)user_data;

    if (capacity == 0)
    {
        return len;
    }

    if (copy > capacity - 1)
    {
        copy = capacity - 1;
    }
    memcpy(out, text, copy);
    out[copy] = '\0';

    return len;
}

static void reset_spy(void)
{
    seen_value[0] = '\0';
    seen_arg[0] = '\0';
    seen_user_data = NULL;
    seen_calls = 0;
}

TEST(test_formatter_runs_for_a_placeholder)
{
    char out[64];

    ci18n_init();
    ASSERT(ci18n_set_formatter("upper", fmt_upper, NULL));

    ci18n_set("en", "greet", "Hello, {name:upper}!");
    ci18n_set_current("en");

    ci18n_format(out, sizeof(out), "greet", "name", "world", NULL);
    ASSERT_STR_EQ(out, "Hello, WORLD!");
    ASSERT(ci18n_last_error() == CI18N_OK);

    ci18n_free();
}

TEST(test_formatter_receives_value_arg_and_user_data)
{
    char out[64];
    int marker = 7;

    ci18n_init();
    reset_spy();
    ASSERT(ci18n_set_formatter("spy", fmt_spy, &marker));

    ci18n_set("en", "k", "[{v:spy,long,with,commas}]");
    ci18n_set_current("en");

    ci18n_format(out, sizeof(out), "k", "v", "the value", NULL);

    ASSERT(seen_calls == 1);
    ASSERT_STR_EQ(seen_value, "the value");
    /* Everything after the first comma is the argument, verbatim. */
    ASSERT_STR_EQ(seen_arg, "long,with,commas");
    ASSERT(seen_user_data == &marker);
    ASSERT_STR_EQ(out, "[]");

    /* No argument at all means an empty one, never NULL. */
    reset_spy();
    ci18n_set("en", "k2", "{v:spy}");
    ci18n_format(out, sizeof(out), "k2", "v", "x", NULL);
    ASSERT(seen_calls == 1);
    ASSERT_STR_EQ(seen_arg, "");

    ci18n_free();
}

TEST(test_formatter_argument_reaches_the_formatter)
{
    char out[64];

    ci18n_init();
    ASSERT(ci18n_set_formatter("repeat", fmt_repeat, NULL));

    ci18n_set("en", "k", "{v:repeat,3}");
    ci18n_set_current("en");

    ci18n_format(out, sizeof(out), "k", "v", "ab", NULL);
    ASSERT_STR_EQ(out, "ababab");

    ci18n_free();
}

TEST(test_unknown_formatter_is_visible_and_reported)
{
    char out[64];

    ci18n_init();
    ci18n_set("en", "k", "before {v:nosuch} after");
    ci18n_set_current("en");

    ci18n_format(out, sizeof(out), "k", "v", "x", NULL);

    /* The placeholder stays put, so the mistake is on screen rather than
     * silently swallowed, and the rest of the sentence still renders. */
    ASSERT_STR_EQ(out, "before {v:nosuch} after");
    ASSERT(ci18n_last_error() == CI18N_ERR_UNKNOWN_FORMATTER);

    /* Removing a formatter puts a translation back in that state. */
    ASSERT(ci18n_set_formatter("nosuch", fmt_upper, NULL));
    ci18n_format(out, sizeof(out), "k", "v", "x", NULL);
    ASSERT_STR_EQ(out, "before X after");
    ASSERT(ci18n_last_error() == CI18N_OK);

    ASSERT(ci18n_remove_formatter("nosuch"));
    ci18n_format(out, sizeof(out), "k", "v", "x", NULL);
    ASSERT_STR_EQ(out, "before {v:nosuch} after");
    ASSERT(ci18n_last_error() == CI18N_ERR_UNKNOWN_FORMATTER);

    ci18n_free();
}

TEST(test_placeholders_without_a_formatter_are_unchanged)
{
    char out[64];

    ci18n_init();
    ASSERT(ci18n_set_formatter("upper", fmt_upper, NULL));

    ci18n_set("en", "plain", "Hello, {name}!");
    ci18n_set("en", "braces", "{{literal}} and {name}");
    ci18n_set("en", "missing", "Hello, {nobody}!");
    ci18n_set_current("en");

    ci18n_format(out, sizeof(out), "plain", "name", "world", NULL);
    ASSERT_STR_EQ(out, "Hello, world!");
    ASSERT(ci18n_last_error() == CI18N_OK);

    ci18n_format(out, sizeof(out), "braces", "name", "x", NULL);
    ASSERT_STR_EQ(out, "{literal} and x");

    /* An unsupplied name is still left visible, and is not a formatter
     * problem. */
    ci18n_format(out, sizeof(out), "missing", "name", "x", NULL);
    ASSERT_STR_EQ(out, "Hello, {nobody}!");
    ASSERT(ci18n_last_error() == CI18N_OK);

    ci18n_free();
}

TEST(test_formatter_measuring_and_truncation)
{
    char out[8];
    size_t needed;

    ci18n_init();
    ASSERT(ci18n_set_formatter("repeat", fmt_repeat, NULL));
    ci18n_set("en", "k", "{v:repeat,4}");
    ci18n_set_current("en");

    /* Measuring: no buffer, so the formatter must be asked without one. */
    needed = ci18n_format(NULL, 0, "k", "v", "abc", NULL);
    ASSERT(needed == 12);

    /* The same answer with a buffer too small to hold it. */
    needed = ci18n_format(out, sizeof(out), "k", "v", "abc", NULL);
    ASSERT(needed == 12);
    ASSERT(strlen(out) == 7);
    ASSERT_STR_EQ(out, "abcabca");

    ci18n_free();
}

TEST(test_formatter_output_is_cut_at_a_character_boundary)
{
    char out[8];
    size_t needed;

    ci18n_init();
    ASSERT(ci18n_set_formatter("cyr", fmt_cyrillic, NULL));
    ci18n_set("en", "k", "{v:cyr}");
    ci18n_set_current("en");

    /* The formatter produces 12 bytes of 2-byte characters into 7 bytes of
     * room, so the library has to drop the half character it left behind. */
    needed = ci18n_format(out, sizeof(out), "k", "v", "ignored", NULL);
    ASSERT(needed == 12);
    ASSERT(strlen(out) == 6);
    ASSERT_STR_EQ(out, "При");
    ASSERT(ci18n_utf8_valid(out));

    ci18n_free();
}

TEST(test_formatter_works_with_plurals_and_count)
{
    char out[64];

    ci18n_init();
    ASSERT(ci18n_set_formatter("repeat", fmt_repeat, NULL));

    ci18n_set("en", "files[one]", "{count} file for {who:repeat,2}");
    ci18n_set("en", "files[other]", "{count} files for {who:repeat,2}");
    ci18n_set_current("en");

    ci18n_format_plural(out, sizeof(out), "files", 1, "who", "ab", NULL);
    ASSERT_STR_EQ(out, "1 file for abab");

    ci18n_format_plural(out, sizeof(out), "files", 5, "who", "ab", NULL);
    ASSERT_STR_EQ(out, "5 files for abab");

    /* The count itself can go through a formatter too. */
    ci18n_set("en", "n[other]", "<{count:repeat,2}>");
    ci18n_format_plural(out, sizeof(out), "n", 7, NULL);
    ASSERT_STR_EQ(out, "<77>");

    ci18n_free();
}

TEST(test_formatter_registration_is_validated)
{
    ci18n_init();

    ASSERT(!ci18n_set_formatter(NULL, fmt_upper, NULL));
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    ASSERT(!ci18n_set_formatter("", fmt_upper, NULL));
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    ASSERT(!ci18n_set_formatter("upper", NULL, NULL));
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    /* Longer than CI18N_MAX_FORMATTER_NAME can hold. */
    ASSERT(!ci18n_set_formatter("a_name_far_longer_than_the_limit_allows",
                                fmt_upper, NULL));
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    /* Punctuation that the placeholder syntax uses could never be matched,
     * so registering it is a mistake rather than a formatter that never
     * runs. */
    ASSERT(!ci18n_set_formatter("da:te", fmt_upper, NULL));
    ASSERT(!ci18n_set_formatter("da,te", fmt_upper, NULL));
    ASSERT(!ci18n_set_formatter("da}te", fmt_upper, NULL));
    ASSERT(!ci18n_set_formatter("da{te", fmt_upper, NULL));

    /* Removing one that was never registered says so. */
    ASSERT(!ci18n_remove_formatter("never_registered"));
    ASSERT(ci18n_last_error() == CI18N_ERR_KEY_NOT_FOUND);

    ci18n_free();
}

TEST(test_formatter_table_is_bounded_and_replaceable)
{
    char name[CI18N_MAX_FORMATTER_NAME];
    size_t i;
    char out[64];

    ci18n_init();

    for (i = 0; i < CI18N_MAX_FORMATTERS; i++)
    {
        sprintf(name, "f%u", (unsigned)i);
        ASSERT(ci18n_set_formatter(name, fmt_upper, NULL));
    }

    /* One more than the table holds. */
    ASSERT(!ci18n_set_formatter("one_too_many", fmt_upper, NULL));
    ASSERT(ci18n_last_error() == CI18N_ERR_TOO_MANY_FORMATTERS);

    /* Replacing an existing name is not a new entry, so it still fits. */
    ASSERT(ci18n_set_formatter("f0", fmt_repeat, NULL));

    ci18n_set("en", "k", "{v:f0,2}");
    ci18n_set_current("en");
    ci18n_format(out, sizeof(out), "k", "v", "xy", NULL);
    ASSERT_STR_EQ(out, "xyxy");

    /* Freeing a slot lets a new one in. */
    ASSERT(ci18n_remove_formatter("f1"));
    ASSERT(ci18n_set_formatter("one_too_many", fmt_upper, NULL));

    ci18n_free();
}

TEST(test_formatter_argument_too_long_is_a_parse_error)
{
    char out[128];
    char pattern[CI18N_MAX_FORMATTER_ARG + 32];
    size_t i;

    ci18n_init();
    ASSERT(ci18n_set_formatter("spy", fmt_spy, NULL));

    strcpy(pattern, "{v:spy,");
    for (i = strlen(pattern); i < sizeof(pattern) - 3; i++)
    {
        pattern[i] = 'x';
    }
    pattern[sizeof(pattern) - 3] = '}';
    pattern[sizeof(pattern) - 2] = '\0';

    ci18n_set("en", "k", pattern);
    ci18n_set_current("en");

    reset_spy();
    ci18n_format(out, sizeof(out), "k", "v", "value", NULL);

    /* Not shortened behind the translator's back: the placeholder stays
     * visible and the load is reported as malformed. */
    ASSERT(seen_calls == 0);
    ASSERT(ci18n_last_error() == CI18N_ERR_PARSE);
    ASSERT(strstr(out, "{v:spy,") != NULL);

    ci18n_free();
}

TEST(test_formatters_belong_to_their_catalogue)
{
    ci18n_t *catalog = ci18n_create();
    char out[64];

    ASSERT(catalog != NULL);

    ci18n_init();
    ci18n_set("en", "k", "{v:upper}");
    ci18n_set_current("en");

    ci18n_set_in(catalog, "en", "k", "{v:upper}");
    ci18n_set_current_in(catalog, "en");

    /* Registered on the catalogue only. */
    ASSERT(ci18n_set_formatter_in(catalog, "upper", fmt_upper, NULL));

    ci18n_format_in(catalog, out, sizeof(out), "k", "v", "hi", NULL);
    ASSERT_STR_EQ(out, "HI");
    ASSERT(ci18n_last_error_in(catalog) == CI18N_OK);

    /* The default catalogue has no such formatter and does not inherit one. */
    ci18n_format(out, sizeof(out), "k", "v", "hi", NULL);
    ASSERT_STR_EQ(out, "{v:upper}");
    ASSERT(ci18n_last_error() == CI18N_ERR_UNKNOWN_FORMATTER);

    /* And the other way round. */
    ASSERT(ci18n_set_formatter("repeat", fmt_repeat, NULL));
    ASSERT(ci18n_remove_formatter_in(catalog, "upper") == true);
    ASSERT(ci18n_set_formatter_in(catalog, "upper", fmt_upper, NULL));

    ci18n_destroy(catalog);
    ci18n_free();
}

TEST(test_format_in_matches_format)
{
    ci18n_t *catalog = ci18n_create();
    char mine[64];
    char theirs[64];

    ASSERT(catalog != NULL);
    ASSERT(ci18n_set_formatter_in(catalog, "upper", fmt_upper, NULL));

    ci18n_set_in(catalog, "en", "greet", "Hello, {name:upper}!");
    ci18n_set_in(catalog, "en", "files[one]", "{count} file");
    ci18n_set_in(catalog, "en", "files[other]", "{count} files");
    ci18n_set_current_in(catalog, "en");

    ci18n_init();
    ASSERT(ci18n_set_formatter("upper", fmt_upper, NULL));
    ci18n_set("en", "greet", "Hello, {name:upper}!");
    ci18n_set("en", "files[one]", "{count} file");
    ci18n_set("en", "files[other]", "{count} files");
    ci18n_set_current("en");

    ci18n_format(mine, sizeof(mine), "greet", "name", "world", NULL);
    ci18n_format_in(catalog, theirs, sizeof(theirs), "greet", "name", "world", NULL);
    ASSERT_STR_EQ(mine, theirs);
    ASSERT_STR_EQ(theirs, "Hello, WORLD!");

    ci18n_format_plural(mine, sizeof(mine), "files", 3, NULL);
    ci18n_format_plural_in(catalog, theirs, sizeof(theirs), "files", 3, NULL);
    ASSERT_STR_EQ(mine, theirs);
    ASSERT_STR_EQ(theirs, "3 files");

    /* A NULL catalogue answers rather than crashing. */
    ASSERT(ci18n_format_in(NULL, theirs, sizeof(theirs), "greet", NULL) == 0);
    ASSERT(ci18n_format_plural_in(NULL, theirs, sizeof(theirs), "files", 1, NULL) == 0);

    ci18n_destroy(catalog);
    ci18n_free();
}

TEST(test_formatters_are_cleared_by_free)
{
    char out[64];

    ci18n_init();
    ASSERT(ci18n_set_formatter("upper", fmt_upper, NULL));
    ci18n_free();

    /* A new run starts with an empty table rather than inheriting the last
     * one's callbacks, which might point at freed state. */
    ci18n_init();
    ci18n_set("en", "k", "{v:upper}");
    ci18n_set_current("en");

    ci18n_format(out, sizeof(out), "k", "v", "hi", NULL);
    ASSERT_STR_EQ(out, "{v:upper}");
    ASSERT(ci18n_last_error() == CI18N_ERR_UNKNOWN_FORMATTER);

    ci18n_free();
}

TEST(test_formatter_needs_init)
{
    ci18n_free();

    ASSERT(!ci18n_set_formatter("upper", fmt_upper, NULL));
    ASSERT(ci18n_last_error() == CI18N_ERR_NOT_INITIALIZED);

    ASSERT(!ci18n_remove_formatter("upper"));
    ASSERT(ci18n_last_error() == CI18N_ERR_NOT_INITIALIZED);
}

TEST(test_plural_lookup_russian)
{
    ci18n_init();

    ci18n_set("ru", "files[one]", "%d файл");
    ci18n_set("ru", "files[few]", "%d файла");
    ci18n_set("ru", "files[many]", "%d файлов");
    ci18n_set_current("ru");

    ASSERT_STR_EQ(ci18n_plural("files", 1), "%d файл");
    ASSERT_STR_EQ(ci18n_plural("files", 2), "%d файла");
    ASSERT_STR_EQ(ci18n_plural("files", 5), "%d файлов");
    ASSERT_STR_EQ(ci18n_plural("files", 11), "%d файлов");
    ASSERT_STR_EQ(ci18n_plural("files", 21), "%d файл");

    ci18n_free();
}

TEST(test_plural_falls_back_through_other_then_plain)
{
    ci18n_init();

    /* Only the catch-all form exists, so every count lands on it. */
    ci18n_set("ru", "items[other]", "items catch-all");
    ci18n_set_current("ru");
    ASSERT_STR_EQ(ci18n_plural("items", 1), "items catch-all");
    ASSERT_STR_EQ(ci18n_plural("items", 5), "items catch-all");

    /* The exact form wins once it is there. */
    ci18n_set("ru", "items[one]", "items one");
    ASSERT_STR_EQ(ci18n_plural("items", 1), "items one");
    ASSERT_STR_EQ(ci18n_plural("items", 5), "items catch-all");

    /* A key with no plural forms at all still answers. */
    ci18n_set("ru", "plain", "no forms here");
    ASSERT_STR_EQ(ci18n_plural("plain", 3), "no forms here");

    /* And a key that does not exist reports as much. */
    ASSERT(ci18n_plural("absent", 1) == NULL);
    ASSERT(ci18n_last_error() == CI18N_ERR_KEY_NOT_FOUND);
    ASSERT_STR_EQ(ci18n_plural_or_key("absent", 1), "absent");

    ci18n_free();
}

TEST(test_plural_guards)
{
    ASSERT(ci18n_plural("k", 1) == NULL);
    ASSERT(ci18n_last_error() == CI18N_ERR_NOT_INITIALIZED);

    ci18n_init();
    ASSERT(ci18n_plural(NULL, 1) == NULL);
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    /* A key too long to hold a bracket suffix must not overflow the buffer;
     * it simply cannot match a plural form. */
    {
        char long_key[CI18N_MAX_KEY_LENGTH];

        memset(long_key, 'k', sizeof(long_key) - 1);
        long_key[sizeof(long_key) - 1] = '\0';

        ci18n_set("en", long_key, "plain value");
        ci18n_set_current("en");
        ASSERT_STR_EQ(ci18n_plural(long_key, 2), "plain value");
    }

    ci18n_free();
}

TEST(test_remove_language_frees_the_slot)
{
    char code[CI18N_MAX_CODE_LENGTH];
    size_t i;

    ci18n_init();

    ci18n_set("en", "k", "english");
    ci18n_set("ru", "k", "russian");
    ci18n_set("es", "k", "spanish");
    ASSERT(ci18n_get_languages(NULL, 0) == 3);

    ASSERT(ci18n_remove_language("ru") == true);
    ASSERT(ci18n_get_languages(NULL, 0) == 2);
    ASSERT(ci18n_count("ru") == 0);

    /* The survivors are still findable after the last one moved into the
     * hole, which is the part a shift would have got wrong. */
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("k"), "english");
    ci18n_set_current("es");
    ASSERT_STR_EQ(ci18n_get("k"), "spanish");

    /* Removing what is not there is a miss, not a corruption. */
    ASSERT(ci18n_remove_language("ru") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_LANGUAGE_NOT_FOUND);

    /* The slot is genuinely released, unlike ci18n_clear(): filling the
     * table, removing one and adding another has to work. */
    ci18n_free();
    ci18n_init();

    for (i = 0; i < CI18N_MAX_LANGUAGES; i++)
    {
        snprintf(code, sizeof(code), "l%u", (unsigned int)i);
        ASSERT(ci18n_set(code, "k", "v") == true);
    }

    ASSERT(ci18n_set("overflow", "k", "v") == false);
    ASSERT(ci18n_remove_language("l0") == true);
    ASSERT(ci18n_set("overflow", "k", "v") == true);
    ASSERT(ci18n_get_languages(NULL, 0) == CI18N_MAX_LANGUAGES);

    ci18n_free();
}

TEST(test_remove_language_clears_the_selection)
{
    ci18n_init();

    ci18n_set("en", "k", "english");
    ci18n_set("ru", "k", "russian");
    ci18n_set_current("ru");
    ci18n_set_fallback("en");

    ASSERT(ci18n_remove_language("ru") == true);

    /* Pointing at a language that no longer exists would be worse than
     * pointing at nothing. */
    ASSERT_STR_EQ(ci18n_get_current(), "");

    /* The fallback still answers, since it was a different language. */
    ASSERT_STR_EQ(ci18n_get("k"), "english");

    ASSERT(ci18n_remove_language("en") == true);
    ASSERT(ci18n_get("k") == NULL);
    ASSERT(ci18n_get_languages(NULL, 0) == 0);

    ci18n_free();
}

TEST(test_remove_language_guards)
{
    ASSERT(ci18n_remove_language("en") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_NOT_INITIALIZED);

    ci18n_init();
    ASSERT(ci18n_remove_language(NULL) == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);
    ci18n_free();
}

/* ============================================================================
 * Catalogues
 *
 * The reason the handle exists: two independent sets of translations in one
 * program, so a library using ci18n cannot steal the application's language.
 * ============================================================================ */

TEST(test_catalogues_are_independent)
{
    ci18n_t *ui = ci18n_create();
    ci18n_t *logs = ci18n_create();

    ASSERT(ui != NULL);
    ASSERT(logs != NULL);

    ASSERT(ci18n_set_in(ui, "en", "k", "ui english") == true);
    ASSERT(ci18n_set_in(ui, "ru", "k", "ui russian") == true);
    ASSERT(ci18n_set_in(logs, "en", "k", "log english") == true);

    /* Separate selections. This is the whole point: a component switching its
     * own language must not move anybody else's. */
    ASSERT(ci18n_set_current_in(ui, "ru") == true);
    ASSERT(ci18n_set_current_in(logs, "en") == true);

    ASSERT_STR_EQ(ci18n_get_in(ui, "k"), "ui russian");
    ASSERT_STR_EQ(ci18n_get_in(logs, "k"), "log english");

    /* Separate language tables too. */
    ASSERT(ci18n_get_languages_in(ui, NULL, 0) == 2);
    ASSERT(ci18n_get_languages_in(logs, NULL, 0) == 1);
    ASSERT(ci18n_count_in(logs, "ru") == 0);

    /* And the default catalogue is a third, untouched by either. */
    ci18n_init();
    ASSERT(ci18n_get_languages(NULL, 0) == 0);
    ASSERT(ci18n_get("k") == NULL);

    ci18n_destroy(ui);
    ci18n_destroy(logs);
    ci18n_free();
}

TEST(test_catalogue_covers_the_whole_api)
{
    ci18n_t *cat = ci18n_create();
    char buffer[64];
    const char *codes[4];

    ASSERT(cat != NULL);

    /* Loading, plurals, interpolation and diagnostics all work on a
     * catalogue, not only on the default one. */
    ASSERT(ci18n_load_from_buffer_in(cat, "ru",
                                     "files[one]={count} файл\n"
                                     "files[few]={count} файла\n"
                                     "files[many]={count} файлов\n"
                                     "greeting=Привет\n",
                                     strlen("files[one]={count} файл\n"
                                            "files[few]={count} файла\n"
                                            "files[many]={count} файлов\n"
                                            "greeting=Привет\n")) == true);

    ASSERT(ci18n_set_current_in(cat, "ru") == true);
    ASSERT(ci18n_count_in(cat, "ru") == 4);
    ASSERT(ci18n_has_in(cat, "greeting") == true);
    ASSERT_STR_EQ(ci18n_get_current_in(cat), "ru");
    ASSERT_STR_EQ(ci18n_get_or_key_in(cat, "absent"), "absent");

    ASSERT(ci18n_get_copy_in(cat, "greeting", buffer, sizeof(buffer)) == 12);
    ASSERT_STR_EQ(buffer, "Привет");

    ASSERT_STR_EQ(ci18n_plural_in(cat, "files", 2), "{count} файла");
    ASSERT_STR_EQ(ci18n_plural_or_key_in(cat, "files", 5), "{count} файлов");

    ASSERT(ci18n_get_languages_in(cat, codes, 4) == 1);
    ASSERT_STR_EQ(codes[0], "ru");

    /* The catalogue's own error slot, which the shared mode keeps separate
     * from the per-thread one. */
    ASSERT(ci18n_get_in(cat, "absent") == NULL);
    ASSERT(ci18n_last_error_in(cat) == CI18N_ERR_KEY_NOT_FOUND);
    ASSERT(ci18n_last_load_stats_in(cat)->entries_loaded == 4);

    ASSERT(ci18n_remove_in(cat, "ru", "greeting") == true);
    ASSERT(ci18n_count_in(cat, "ru") == 3);
    ASSERT(ci18n_clear_in(cat, "ru") == true);
    ASSERT(ci18n_count_in(cat, "ru") == 0);
    ASSERT(ci18n_remove_language_in(cat, "ru") == true);
    ASSERT(ci18n_get_languages_in(cat, NULL, 0) == 0);

    ci18n_destroy(cat);
}

TEST(test_default_catalogue_is_reachable)
{
    ci18n_init();
    ci18n_set("en", "k", "through the plain api");
    ci18n_set_current("en");

    /* The plain functions are the _in functions on this catalogue, so both
     * spellings have to see the same thing. */
    ASSERT_STR_EQ(ci18n_get_in(ci18n_default(), "k"), "through the plain api");
    ASSERT(ci18n_set_in(ci18n_default(), "en", "other", "v") == true);
    ASSERT_STR_EQ(ci18n_get("other"), "v");

    ci18n_free();
}

TEST(test_destroy_tolerates_null)
{
    /* So a failed create needs no special case at the call site. */
    ci18n_destroy(NULL);
}

/* ============================================================================
 * Escape sequences
 *
 * Decoded by the parser only. ci18n_set() takes strings the C compiler has
 * already unescaped, so decoding there would corrupt a value that genuinely
 * contains a backslash.
 * ============================================================================ */

TEST(test_escapes_in_values)
{
    ci18n_init();

    ASSERT(ci18n_load_from_buffer("en",
                                  "newline=a\\nb\n"
                                  "tab=a\\tb\n"
                                  "carriage=a\\rb\n"
                                  "backslash=a\\\\b\n"
                                  "equals=a\\=b\n",
                                  strlen("newline=a\\nb\n"
                                         "tab=a\\tb\n"
                                         "carriage=a\\rb\n"
                                         "backslash=a\\\\b\n"
                                         "equals=a\\=b\n")) == true);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("newline"), "a\nb");
    ASSERT_STR_EQ(ci18n_get("tab"), "a\tb");
    ASSERT_STR_EQ(ci18n_get("carriage"), "a\rb");
    ASSERT_STR_EQ(ci18n_get("backslash"), "a\\b");
    ASSERT_STR_EQ(ci18n_get("equals"), "a=b");

    ci18n_free();
}

TEST(test_escaped_separator_in_key)
{
    const char *buffer = "we\\=ird=value\nplain=other\n";

    ci18n_init();
    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_set_current("en");

    /* The key holds a real equals sign, and the split happened at the first
     * unescaped one. */
    ASSERT_STR_EQ(ci18n_get("we=ird"), "value");
    ASSERT(ci18n_get("we\\=ird") == NULL);
    ASSERT_STR_EQ(ci18n_get("plain"), "other");

    ci18n_free();
}

TEST(test_escapes_in_key_and_value_together)
{
    const char *buffer = "we\\=ird=a\\nb\nplain\\=key=x\\ty\n";

    ci18n_init();
    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);
    ci18n_set_current("en");

    /* Decoding the key used to switch decoding off for its own value,
     * because one flag was doing both jobs. Nothing caught it until a real
     * .po file arrived with escapes on both sides of the separator. */
    ASSERT_STR_EQ(ci18n_get("we=ird"), "a\nb");
    ASSERT_STR_EQ(ci18n_get("plain=key"), "x\ty");

    ci18n_free();
}

TEST(test_escaped_comment_markers)
{
    const char *buffer = "\\#hash=value one\n\\;semi=value two\n# real comment\n";

    ci18n_init();
    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);
    ASSERT(ci18n_count("en") == 2);

    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("#hash"), "value one");
    ASSERT_STR_EQ(ci18n_get(";semi"), "value two");

    ci18n_free();
}

TEST(test_escaped_space_survives_trimming)
{
    const char *buffer = "padded=value\\ \ntrimmed=value   \n";

    ci18n_init();
    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);

    ci18n_set_current("en");

    /* An escaped trailing space is content; an unescaped one is padding. This
     * is the only way to keep whitespace the trimmer would otherwise eat. */
    ASSERT_STR_EQ(ci18n_get("padded"), "value ");
    ASSERT_STR_EQ(ci18n_get("trimmed"), "value");

    ci18n_free();
}

TEST(test_unknown_escape_is_left_alone)
{
    const char *buffer = "weird=a\\qb\ntrailing=ends with\\\n";

    ci18n_init();
    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);

    ci18n_set_current("en");

    /* An unrecognised sequence keeps both characters, so a stray backslash
     * stays visible rather than deleting the letter after it. */
    ASSERT_STR_EQ(ci18n_get("weird"), "a\\qb");

    /* A backslash at the very end is just a backslash. */
    ASSERT_STR_EQ(ci18n_get("trailing"), "ends with\\");

    ci18n_free();
}

TEST(test_set_does_not_decode_escapes)
{
    ci18n_init();

    /* The C compiler already turned this source text into a backslash and an
     * n. Decoding again would turn a Windows path into a line break. */
    ci18n_set("en", "path", "C:\\new\\table");
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("path"), "C:\\new\\table");

    /* And a key with a backslash is stored as written. */
    ci18n_set("en", "a\\b", "v");
    ASSERT_STR_EQ(ci18n_get("a\\b"), "v");

    ci18n_free();
}

TEST(test_escapes_survive_update_paths)
{
    ci18n_init();
    ci18n_set_current("en");

    /* Shorter replacement, written into the existing slot. */
    ASSERT(ci18n_load_from_buffer("en", "k=aaaa\\nbbbb\n", 13) == true);
    ci18n_set_current("en");
    ASSERT_STR_EQ(ci18n_get("k"), "aaaa\nbbbb");

    ASSERT(ci18n_load_from_buffer("en", "k=x\\ny\n", 7) == true);
    ASSERT_STR_EQ(ci18n_get("k"), "x\ny");

    /* Longer replacement, appended to the arena. */
    ASSERT(ci18n_load_from_buffer("en", "k=much\\tlonger\\tvalue\\there\n", 27) == true);
    ASSERT_STR_EQ(ci18n_get("k"), "much\tlonger\tvalue\there");

    ASSERT(ci18n_count("en") == 1);

    ci18n_free();
}

/* ============================================================================
 * Interpolation
 * ============================================================================ */

TEST(test_format_substitutes_by_name)
{
    char out[128];

    ci18n_init();
    ci18n_set("en", "greeting", "Hello, {name}! You have {count} messages.");
    ci18n_set_current("en");

    ASSERT(ci18n_format(out, sizeof(out), "greeting",
                        "name", "Ilya", "count", "3", NULL) == 33);
    ASSERT_STR_EQ(out, "Hello, Ilya! You have 3 messages.");

    ci18n_free();
}

TEST(test_format_order_is_the_translations_business)
{
    char out[128];

    ci18n_init();

    /* The same two values, placed differently by each translation. This is
     * the thing positional formatting cannot express. */
    ci18n_set("en", "moved", "{first} then {second}");
    ci18n_set("ru", "moved", "{second}, а раньше {first}");

    ci18n_set_current("en");
    ci18n_format(out, sizeof(out), "moved", "first", "A", "second", "B", NULL);
    ASSERT_STR_EQ(out, "A then B");

    ci18n_set_current("ru");
    ci18n_format(out, sizeof(out), "moved", "first", "A", "second", "B", NULL);
    ASSERT_STR_EQ(out, "B, а раньше A");

    ci18n_free();
}

TEST(test_format_repeated_unused_and_missing)
{
    char out[128];

    ci18n_init();

    /* Used twice. ci18n_set_current() only works once the language exists,
     * so the first entry has to come first. */
    ci18n_set("en", "twice", "{x} and {x}");
    ci18n_set_current("en");
    ci18n_format(out, sizeof(out), "twice", "x", "same", NULL);
    ASSERT_STR_EQ(out, "same and same");

    /* A pair the translation never mentions is simply not used. */
    ci18n_set("en", "one_only", "just {x}");
    ci18n_format(out, sizeof(out), "one_only", "x", "this", "unused", "value", NULL);
    ASSERT_STR_EQ(out, "just this");

    /* No matching pair: the placeholder stays visible rather than vanishing,
     * so a typo in a translation is something you can see. */
    ci18n_set("en", "typo", "hello {nmae}");
    ci18n_format(out, sizeof(out), "typo", "name", "Ilya", NULL);
    ASSERT_STR_EQ(out, "hello {nmae}");

    /* A NULL value is an empty string, not a crash. */
    ci18n_set("en", "nullable", "[{x}]");
    ci18n_format(out, sizeof(out), "nullable", "x", NULL, NULL);
    ASSERT_STR_EQ(out, "[]");

    ci18n_free();
}

TEST(test_format_braces_and_malformed_placeholders)
{
    char out[128];

    ci18n_init();

    ci18n_set("en", "escaped", "{{x}} stays, {x} does not");
    ci18n_set_current("en");
    ci18n_format(out, sizeof(out), "escaped", "x", "V", NULL);
    ASSERT_STR_EQ(out, "{x} stays, V does not");

    /* A lone closing brace is literal. */
    ci18n_set("en", "lone_close", "a } b");
    ci18n_format(out, sizeof(out), "lone_close", NULL);
    ASSERT_STR_EQ(out, "a } b");

    /* An unterminated placeholder is literal too, rather than swallowing the
     * rest of the string into a name. */
    ci18n_set("en", "unterminated", "a {b c");
    ci18n_format(out, sizeof(out), "unterminated", "b", "X", NULL);
    ASSERT_STR_EQ(out, "a {b c");

    /* An empty name matches nothing and stays as written. */
    ci18n_set("en", "empty_name", "a {} b");
    ci18n_format(out, sizeof(out), "empty_name", "", "X", NULL);
    ASSERT_STR_EQ(out, "a X b");

    ci18n_free();
}

TEST(test_format_truncation_and_measuring)
{
    char out[8];
    size_t needed;

    ci18n_init();
    ci18n_set("en", "long", "0123456789abcdef");
    ci18n_set_current("en");

    /* snprintf semantics: the full length comes back, the buffer holds what
     * fits, and it is always terminated. */
    needed = ci18n_format(out, sizeof(out), "long", NULL);
    ASSERT(needed == 16);
    ASSERT(strlen(out) == 7);
    ASSERT_STR_EQ(out, "0123456");

    /* Measuring without writing, which is how a caller sizes a buffer. */
    ASSERT(ci18n_format(NULL, 0, "long", NULL) == 16);

    /* A capacity of one leaves room for the terminator alone. */
    needed = ci18n_format(out, 1, "long", NULL);
    ASSERT(needed == 16);
    ASSERT(out[0] == '\0');

    /* A key that does not exist writes nothing and reports nothing. */
    out[0] = 'x';
    ASSERT(ci18n_format(out, sizeof(out), "absent", NULL) == 0);
    ASSERT(out[0] == '\0');

    ci18n_free();
}

TEST(test_format_plural_provides_count)
{
    char out[128];

    ci18n_init();

    ci18n_set("ru", "files[one]", "{count} файл");
    ci18n_set("ru", "files[few]", "{count} файла");
    ci18n_set("ru", "files[many]", "{count} файлов");
    ci18n_set_current("ru");

    /* The count fills {count} without being passed as a pair, and picks the
     * form at the same time. */
    ci18n_format_plural(out, sizeof(out), "files", 1, NULL);
    ASSERT_STR_EQ(out, "1 файл");

    ci18n_format_plural(out, sizeof(out), "files", 2, NULL);
    ASSERT_STR_EQ(out, "2 файла");

    ci18n_format_plural(out, sizeof(out), "files", 5, NULL);
    ASSERT_STR_EQ(out, "5 файлов");

    ci18n_format_plural(out, sizeof(out), "files", 21, NULL);
    ASSERT_STR_EQ(out, "21 файл");

    ci18n_format_plural(out, sizeof(out), "files", 111, NULL);
    ASSERT_STR_EQ(out, "111 файлов");

    /* Negative counts render with a sign and use the absolute value's form. */
    ci18n_format_plural(out, sizeof(out), "files", -2, NULL);
    ASSERT_STR_EQ(out, "-2 файла");

    /* Zero. */
    ci18n_format_plural(out, sizeof(out), "files", 0, NULL);
    ASSERT_STR_EQ(out, "0 файлов");

    ci18n_free();
}

TEST(test_format_plural_mixes_pairs_and_overrides_count)
{
    char out[128];

    ci18n_init();
    ci18n_set("en", "inbox[one]", "{name} has {count} message");
    ci18n_set("en", "inbox[other]", "{name} has {count} messages");
    ci18n_set_current("en");

    ci18n_format_plural(out, sizeof(out), "inbox", 1, "name", "Ilya", NULL);
    ASSERT_STR_EQ(out, "Ilya has 1 message");

    ci18n_format_plural(out, sizeof(out), "inbox", 7, "name", "Ilya", NULL);
    ASSERT_STR_EQ(out, "Ilya has 7 messages");

    /* An explicit pair wins over the rendered number, so a caller can write
     * "many" or "99+" while still selecting the right form. */
    ci18n_format_plural(out, sizeof(out), "inbox", 7,
                        "name", "Ilya", "count", "lots of", NULL);
    ASSERT_STR_EQ(out, "Ilya has lots of messages");

    /* No such key at all. */
    ASSERT(ci18n_format_plural(out, sizeof(out), "absent", 1, NULL) == 0);
    ASSERT(out[0] == '\0');

    ci18n_free();
}

/* ============================================================================
 * Locale detection
 * ============================================================================ */

TEST(test_set_current_best_walks_the_chain)
{
    ci18n_init();

    ci18n_set("ru", "k", "russian");
    ci18n_set("en", "k", "english");

    /* Exact match. */
    ASSERT(ci18n_set_current_best("ru") == true);
    ASSERT_STR_EQ(ci18n_get("k"), "russian");

    /* A region subtag nobody loaded falls back to the bare language, which is
     * the whole point: load "ru", honour a user asking for "ru-RU". */
    ASSERT(ci18n_set_current_best("ru-RU") == true);
    ASSERT_STR_EQ(ci18n_get_current(), "ru");

    /* Underscores are accepted as well as hyphens. */
    ASSERT(ci18n_set_current_best("en_GB") == true);
    ASSERT_STR_EQ(ci18n_get_current(), "en");

    /* Three subtags deep. */
    ASSERT(ci18n_set_current_best("ru-Cyrl-RU") == true);
    ASSERT_STR_EQ(ci18n_get_current(), "ru");

    ci18n_free();
}

TEST(test_set_current_best_leaves_current_alone_on_failure)
{
    ci18n_init();

    ci18n_set("en", "k", "english");
    ci18n_set_current("en");

    ASSERT(ci18n_set_current_best("de-DE") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_LANGUAGE_NOT_FOUND);

    /* A failed selection must not leave the program with no language. */
    ASSERT_STR_EQ(ci18n_get_current(), "en");
    ASSERT_STR_EQ(ci18n_get("k"), "english");

    ci18n_free();
}

TEST(test_detect_locale)
{
    char buffer[CI18N_MAX_CODE_LENGTH];
    size_t len;

    ci18n_init();

    /* Whatever this machine reports, the result has to be a usable tag or an
     * honest nothing, never junk. */
    len = ci18n_detect_locale(buffer, sizeof(buffer));
    ASSERT(len == strlen(buffer));

    if (len > 0)
    {
        ASSERT(strchr(buffer, '.') == NULL);
        ASSERT(strchr(buffer, '@') == NULL);
        ASSERT(strchr(buffer, '_') == NULL);
        ASSERT(strcmp(buffer, "C") != 0);
        ASSERT(strcmp(buffer, "POSIX") != 0);
    }

    ASSERT(ci18n_detect_locale(NULL, 10) == 0);
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    ci18n_free();
}

/* ============================================================================
 * Diagnostics
 * ============================================================================ */

TEST(test_error_string_covers_every_code)
{
    /* Every enumerator needs its own text, and nothing may fall through to
     * the unknown-error catch-all. */
    ci18n_error_t codes[] = {
        CI18N_OK,
        CI18N_ERR_NOT_INITIALIZED,
        CI18N_ERR_INVALID_ARGUMENT,
        CI18N_ERR_CODE_TOO_LONG,
        CI18N_ERR_FILE_NOT_FOUND,
        CI18N_ERR_OUT_OF_MEMORY,
        CI18N_ERR_TOO_MANY_LANGUAGES,
        CI18N_ERR_TOO_MANY_KEYS,
        CI18N_ERR_LANGUAGE_NOT_FOUND,
        CI18N_ERR_KEY_NOT_FOUND,
        CI18N_ERR_PARSE,
        CI18N_ERR_TOO_MANY_FORMATTERS,
        CI18N_ERR_UNKNOWN_FORMATTER
    };
    size_t i;

    for (i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        const char *text = ci18n_error_string(codes[i]);

        ASSERT(text != NULL);
        ASSERT(strlen(text) > 0);
        ASSERT(strcmp(text, "unknown error") != 0);
    }

    /* Out of range still answers, rather than reading past the switch. */
    ASSERT_STR_EQ(ci18n_error_string((ci18n_error_t)9999), "unknown error");
}

TEST(test_last_error_before_init)
{
    ci18n_init();
    ci18n_free();

    ASSERT(ci18n_set("en", "k", "v") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_NOT_INITIALIZED);

    ASSERT(ci18n_get("k") == NULL);
    ASSERT(ci18n_last_error() == CI18N_ERR_NOT_INITIALIZED);
}

TEST(test_last_error_invalid_argument)
{
    ci18n_init();

    ASSERT(ci18n_set(NULL, "k", "v") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    ASSERT(ci18n_get(NULL) == NULL);
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    ci18n_free();
}

TEST(test_last_error_language_and_key)
{
    ci18n_init();
    ci18n_set("en", "k", "v");
    ci18n_set_current("en");

    ASSERT(ci18n_set_current("nope") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_LANGUAGE_NOT_FOUND);

    ASSERT(ci18n_remove("nope", "k") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_LANGUAGE_NOT_FOUND);

    ASSERT(ci18n_get("missing") == NULL);
    ASSERT(ci18n_last_error() == CI18N_ERR_KEY_NOT_FOUND);

    ASSERT(ci18n_remove("en", "missing") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_KEY_NOT_FOUND);

    ci18n_free();
}

TEST(test_last_error_file_not_found)
{
    ci18n_init();

    ASSERT(ci18n_load_language("en", "definitely_not_here.txt") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_FILE_NOT_FOUND);

    ci18n_free();
}

TEST(test_last_error_limits)
{
    char code[CI18N_MAX_CODE_LENGTH];
    size_t i;

    ci18n_init();

    for (i = 0; i < CI18N_MAX_LANGUAGES; i++)
    {
        snprintf(code, sizeof(code), "l%u", (unsigned int)i);
        ci18n_set(code, "k", "v");
    }

    ASSERT(ci18n_set("overflow", "k", "v") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_TOO_MANY_LANGUAGES);

    ci18n_free();
    ci18n_init();

    for (i = 0; i < CI18N_MAX_KEYS_PER_LANGUAGE; i++)
    {
        snprintf(code, sizeof(code), "k%u", (unsigned int)i);
        ci18n_set("en", code, "v");
    }

    ASSERT(ci18n_set("en", "overflow", "v") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_TOO_MANY_KEYS);

    ci18n_free();
}

TEST(test_success_clears_last_error)
{
    ci18n_init();

    ASSERT(ci18n_set(NULL, "k", "v") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_INVALID_ARGUMENT);

    /* Every successful call has to wipe the previous failure, otherwise the
     * code would linger and describe something the caller already handled. */
    ASSERT(ci18n_set("en", "k", "v") == true);
    ASSERT(ci18n_last_error() == CI18N_OK);

    ci18n_set_current("en");
    ASSERT(ci18n_last_error() == CI18N_OK);

    ASSERT(ci18n_get("missing") == NULL);
    ASSERT(ci18n_last_error() == CI18N_ERR_KEY_NOT_FOUND);

    ASSERT_STR_EQ(ci18n_get("k"), "v");
    ASSERT(ci18n_last_error() == CI18N_OK);

    ci18n_free();
}

TEST(test_has_does_not_report_a_failure)
{
    ci18n_init();
    ci18n_set("en", "k", "v");
    ci18n_set_current("en");

    /* A miss from ci18n_has() is an answer, not an error. */
    ASSERT(ci18n_has("missing") == false);
    ASSERT(ci18n_last_error() == CI18N_OK);

    ASSERT(ci18n_has("k") == true);
    ASSERT(ci18n_last_error() == CI18N_OK);

    ci18n_free();
}

TEST(test_load_stats_clean_file)
{
    const ci18n_load_stats_t *st;

    ci18n_init();

    ASSERT(write_text(TEMP_FILE,
                      "# a comment\n"
                      "\n"
                      "a=one\n"
                      "b=two\n"));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_last_error() == CI18N_OK);

    st = ci18n_last_load_stats();
    ASSERT(st->lines_read == 4);
    ASSERT(st->entries_loaded == 2);
    ASSERT(st->lines_skipped == 2);
    ASSERT(st->lines_malformed == 0);
    ASSERT(st->first_malformed_line == 0);
    ASSERT(st->keys_truncated == 0);
    ASSERT(st->values_truncated == 0);

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_stats_malformed_lines)
{
    const ci18n_load_stats_t *st;

    ci18n_init();

    ASSERT(write_text(TEMP_FILE,
                      "a=one\n"
                      "no separator here\n"
                      "b=two\n"
                      "=empty key\n"));

    /* Still true, because the file was readable. The detail is in the stats,
     * and the error code flags that something was dropped. */
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_last_error() == CI18N_ERR_PARSE);

    st = ci18n_last_load_stats();
    ASSERT(st->lines_read == 4);
    ASSERT(st->entries_loaded == 2);
    ASSERT(st->lines_malformed == 2);
    ASSERT(st->first_malformed_line == 2);

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_stats_reports_truncation)
{
    char line[CI18N_MAX_KEY_LENGTH + CI18N_MAX_VALUE_LENGTH + 128];
    const ci18n_load_stats_t *st;
    size_t pos;

    ci18n_init();

    /* An over-long key, then an over-long value, on separate lines. */
    pos = 0;
    memset(line + pos, 'k', CI18N_MAX_KEY_LENGTH + 16);
    pos += CI18N_MAX_KEY_LENGTH + 16;
    line[pos++] = '=';
    line[pos++] = 'v';
    line[pos++] = '\n';
    line[pos++] = 'j';
    line[pos++] = '=';
    memset(line + pos, 'v', CI18N_MAX_VALUE_LENGTH + 16);
    pos += CI18N_MAX_VALUE_LENGTH + 16;
    line[pos++] = '\n';

    ASSERT(write_file(TEMP_FILE, line, pos));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);
    ASSERT(ci18n_last_error() == CI18N_ERR_PARSE);

    st = ci18n_last_load_stats();

    /* The default line buffer holds a maximum key and a maximum value at
     * once, so each limit is reported by its own counter and neither line is
     * cut as a whole. */
    ASSERT(st->keys_truncated == 1);
    ASSERT(st->values_truncated == 1);
    ASSERT(st->lines_truncated == 0);
    ASSERT(st->lines_read == 2);
    ASSERT(st->entries_loaded == 2);
    ASSERT(st->lines_malformed == 0);

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_stats_over_long_line)
{
    static char line[CI18N_MAX_LINE_LENGTH * 2];
    const ci18n_load_stats_t *st;
    size_t pos = 0;

    ci18n_init();

    /* A single line longer than the whole line buffer, followed by a good
     * one. The tail of the long line used to come back from fgets() as a
     * separate line with no separator and be counted as malformed, inventing
     * a parse error that was not in the file. */
    line[pos++] = 'k';
    line[pos++] = '=';
    memset(line + pos, 'v', CI18N_MAX_LINE_LENGTH + 64);
    pos += CI18N_MAX_LINE_LENGTH + 64;
    line[pos++] = '\n';

    memcpy(line + pos, "good=yes\n", 9);
    pos += 9;

    ASSERT(write_file(TEMP_FILE, line, pos));
    ASSERT(ci18n_load_language("en", TEMP_FILE) == true);

    /* Checked before anything else runs: a successful call clears the code. */
    ASSERT(ci18n_last_error() == CI18N_ERR_PARSE);

    st = ci18n_last_load_stats();
    ASSERT(st->lines_truncated == 1);
    ASSERT(st->lines_read == 2);
    ASSERT(st->lines_malformed == 0);
    ASSERT(st->entries_loaded == 2);

    /* Both entries are there: the long one shortened, the next one intact. */
    ci18n_set_current("en");
    ASSERT(ci18n_get("k") != NULL);
    ASSERT_STR_EQ(ci18n_get("good"), "yes");

    remove(TEMP_FILE);
    ci18n_free();
}

TEST(test_load_stats_from_buffer)
{
    const char *buffer = "a=one\nbad line\nb=two";
    const ci18n_load_stats_t *st;

    ci18n_init();

    /* The buffer loader keeps the same book as the file loader, including the
     * final line when the buffer ends without a terminator. */
    ASSERT(ci18n_load_from_buffer("en", buffer, strlen(buffer)) == true);
    ASSERT(ci18n_last_error() == CI18N_ERR_PARSE);

    st = ci18n_last_load_stats();
    ASSERT(st->lines_read == 3);
    ASSERT(st->entries_loaded == 2);
    ASSERT(st->lines_malformed == 1);
    ASSERT(st->first_malformed_line == 2);

    ci18n_free();
}

TEST(test_load_stats_reset_between_loads)
{
    ci18n_init();

    ASSERT(ci18n_load_from_buffer("en", "bad line", 8) == true);
    ASSERT(ci18n_last_load_stats()->lines_malformed == 1);

    /* A clean load must not inherit the previous one's complaints. */
    ASSERT(ci18n_load_from_buffer("en", "a=one\n", 6) == true);
    ASSERT(ci18n_last_load_stats()->lines_malformed == 0);
    ASSERT(ci18n_last_load_stats()->entries_loaded == 1);
    ASSERT(ci18n_last_error() == CI18N_OK);

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
    RUN_TEST(test_get_languages_count_only);
    RUN_TEST(test_get_languages_small_buffer);
    RUN_TEST(test_get_languages_before_init);
    RUN_TEST(test_utf8_strings);
    RUN_TEST(test_long_value);
    RUN_TEST(test_empty_key_rejected);
    RUN_TEST(test_whitespace_trimming);
    RUN_TEST(test_multiple_equals_in_value);
    RUN_TEST(test_get_current);
    RUN_TEST(test_count_nonexistent_language);

    RUN_TEST(test_load_language_from_file);
    RUN_TEST(test_load_language_missing_file);
    RUN_TEST(test_load_language_empty_file);
    RUN_TEST(test_load_language_crlf);
    RUN_TEST(test_load_language_lone_cr);
    RUN_TEST(test_load_language_no_trailing_newline);
    RUN_TEST(test_load_language_bom);
    RUN_TEST(test_load_language_comments_and_blank_lines);
    RUN_TEST(test_load_language_malformed_lines_are_skipped);
    RUN_TEST(test_load_language_merges_into_existing);
    RUN_TEST(test_load_language_utf8_from_file);

    RUN_TEST(test_null_arguments);
    RUN_TEST(test_calls_before_init);
    RUN_TEST(test_max_languages);
    RUN_TEST(test_max_keys_per_language);
    RUN_TEST(test_long_key_is_truncated);
    RUN_TEST(test_long_value_is_truncated);
    RUN_TEST(test_language_code_at_the_limit);
    RUN_TEST(test_long_language_code_is_rejected);

    RUN_TEST(test_many_keys_all_reachable);
    RUN_TEST(test_remove_keeps_the_rest_reachable);
    RUN_TEST(test_value_update_shorter_and_longer);
    RUN_TEST(test_clear_then_reuse);
    RUN_TEST(test_reload_same_file_twice);

    RUN_TEST(test_plural_category_english);
    RUN_TEST(test_plural_category_russian);
    RUN_TEST(test_plural_category_other_families);
    RUN_TEST(test_plural_negative_counts);
    RUN_TEST(test_plural_category_names);
    RUN_TEST(test_plural_category_ignores_case);

    RUN_TEST(test_direction_right_to_left_languages);
    RUN_TEST(test_direction_left_to_right_languages);
    RUN_TEST(test_direction_script_subtag_wins);
    RUN_TEST(test_direction_reads_only_real_script_subtags);
    RUN_TEST(test_direction_ignores_case);
    RUN_TEST(test_direction_names);
    RUN_TEST(test_current_direction_follows_the_current_language);
    RUN_TEST(test_current_direction_in_a_catalogue);

    RUN_TEST(test_utf8_valid_accepts_well_formed);
    RUN_TEST(test_utf8_valid_rejects_overlong_encodings);
    RUN_TEST(test_utf8_valid_rejects_surrogates_and_out_of_range);
    RUN_TEST(test_utf8_valid_rejects_malformed_sequences);
    RUN_TEST(test_utf8_length_counts_characters);
    RUN_TEST(test_utf8_sequence_length_steps_one_character);
    RUN_TEST(test_utf8_truncate_cuts_at_a_boundary);
    RUN_TEST(test_utf8_decode_respects_its_length_limit);
    RUN_TEST(test_format_truncation_keeps_utf8_intact);

    RUN_TEST(test_formatter_runs_for_a_placeholder);
    RUN_TEST(test_formatter_receives_value_arg_and_user_data);
    RUN_TEST(test_formatter_argument_reaches_the_formatter);
    RUN_TEST(test_unknown_formatter_is_visible_and_reported);
    RUN_TEST(test_placeholders_without_a_formatter_are_unchanged);
    RUN_TEST(test_formatter_measuring_and_truncation);
    RUN_TEST(test_formatter_output_is_cut_at_a_character_boundary);
    RUN_TEST(test_formatter_works_with_plurals_and_count);
    RUN_TEST(test_formatter_registration_is_validated);
    RUN_TEST(test_formatter_table_is_bounded_and_replaceable);
    RUN_TEST(test_formatter_argument_too_long_is_a_parse_error);
    RUN_TEST(test_formatters_belong_to_their_catalogue);
    RUN_TEST(test_format_in_matches_format);
    RUN_TEST(test_formatters_are_cleared_by_free);
    RUN_TEST(test_formatter_needs_init);
    RUN_TEST(test_plural_lookup_russian);
    RUN_TEST(test_plural_falls_back_through_other_then_plain);
    RUN_TEST(test_plural_guards);

    RUN_TEST(test_remove_language_frees_the_slot);
    RUN_TEST(test_remove_language_clears_the_selection);
    RUN_TEST(test_remove_language_guards);

    RUN_TEST(test_catalogues_are_independent);
    RUN_TEST(test_catalogue_covers_the_whole_api);
    RUN_TEST(test_default_catalogue_is_reachable);
    RUN_TEST(test_destroy_tolerates_null);

    RUN_TEST(test_escapes_in_values);
    RUN_TEST(test_escaped_separator_in_key);
    RUN_TEST(test_escapes_in_key_and_value_together);
    RUN_TEST(test_escaped_comment_markers);
    RUN_TEST(test_escaped_space_survives_trimming);
    RUN_TEST(test_unknown_escape_is_left_alone);
    RUN_TEST(test_set_does_not_decode_escapes);
    RUN_TEST(test_escapes_survive_update_paths);

    RUN_TEST(test_format_substitutes_by_name);
    RUN_TEST(test_format_order_is_the_translations_business);
    RUN_TEST(test_format_repeated_unused_and_missing);
    RUN_TEST(test_format_braces_and_malformed_placeholders);
    RUN_TEST(test_format_truncation_and_measuring);
    RUN_TEST(test_format_plural_provides_count);
    RUN_TEST(test_format_plural_mixes_pairs_and_overrides_count);

    RUN_TEST(test_set_current_best_walks_the_chain);
    RUN_TEST(test_set_current_best_leaves_current_alone_on_failure);
    RUN_TEST(test_detect_locale);

    RUN_TEST(test_error_string_covers_every_code);
    RUN_TEST(test_last_error_before_init);
    RUN_TEST(test_last_error_invalid_argument);
    RUN_TEST(test_last_error_language_and_key);
    RUN_TEST(test_last_error_file_not_found);
    RUN_TEST(test_last_error_limits);
    RUN_TEST(test_success_clears_last_error);
    RUN_TEST(test_has_does_not_report_a_failure);
    RUN_TEST(test_load_stats_clean_file);
    RUN_TEST(test_load_stats_malformed_lines);
    RUN_TEST(test_load_stats_reports_truncation);
    RUN_TEST(test_load_stats_over_long_line);
    RUN_TEST(test_load_stats_from_buffer);
    RUN_TEST(test_load_stats_reset_between_loads);

    printf("\n=== Results ===\n");
    printf("Total:   %d\n", tests_run);
    printf("Passed:  %d\n", tests_passed);
    printf("Failed:  %d\n", tests_failed);

    return tests_failed == 0 ? 0 : 1;
}
