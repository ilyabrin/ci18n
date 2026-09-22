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
        sprintf(code, "l%u", (unsigned int)i);
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
        sprintf(key, "k%u", (unsigned int)i);
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
        sprintf(key, "key_%d", i);
        sprintf(value, "value_%d", i);
        ASSERT(ci18n_set("en", key, value) == true);
    }

    ASSERT(ci18n_count("en") == 500);
    ci18n_set_current("en");

    /* Every one of them, not just the last: a rehash that dropped an entry
     * would leave the count right and the lookup wrong. */
    for (i = 0; i < 500; i++)
    {
        sprintf(key, "key_%d", i);
        sprintf(value, "value_%d", i);
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
        sprintf(key, "key_%d", i);
        ci18n_set("en", key, "v");
    }

    ci18n_set_current("en");

    /* Remove every other key. Removal moves the last entry into the hole, so
     * the bucket chains have to be rebuilt around it each time. */
    for (i = 0; i < 100; i += 2)
    {
        sprintf(key, "key_%d", i);
        ASSERT(ci18n_remove("en", key) == true);
    }

    ASSERT(ci18n_count("en") == 50);

    for (i = 0; i < 100; i++)
    {
        sprintf(key, "key_%d", i);

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
        sprintf(key, "key_%d", i);
        ci18n_set("en", key, "first round");
    }

    ASSERT(ci18n_clear("en") == true);
    ASSERT(ci18n_count("en") == 0);

    /* Clearing rewinds the arena, so the second round writes over the first.
     * Everything has to be findable again afterwards. */
    for (i = 0; i < 50; i++)
    {
        sprintf(key, "key_%d", i);
        ASSERT(ci18n_set("en", key, "second round") == true);
    }

    ASSERT(ci18n_count("en") == 50);
    ci18n_set_current("en");

    for (i = 0; i < 50; i++)
    {
        sprintf(key, "key_%d", i);
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
        CI18N_ERR_PARSE
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
        sprintf(code, "l%u", (unsigned int)i);
        ci18n_set(code, "k", "v");
    }

    ASSERT(ci18n_set("overflow", "k", "v") == false);
    ASSERT(ci18n_last_error() == CI18N_ERR_TOO_MANY_LANGUAGES);

    ci18n_free();
    ci18n_init();

    for (i = 0; i < CI18N_MAX_KEYS_PER_LANGUAGE; i++)
    {
        sprintf(code, "k%u", (unsigned int)i);
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
