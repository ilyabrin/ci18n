/*
 * Compiled catalogues read through the copying API, the way a program on
 * AVR has to read them: there the strings stay in flash, and only a copy is
 * ever in RAM. Builds on any target, so the host runs the same checks.
 *
 * Needs en.h and ru.h, which tests/avr/run.sh and `make test-compiled`
 * generate from en.txt and ru.txt next to this file.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"
#include "en.h"
#include "ru.h"

#include <string.h>

#include "console.h"

static int checks;
static int failures;

/* Reports the line and nothing else: on an Uno, the text of every condition
 * would be copied into RAM that the test needs for itself. */
static void check(int ok, const char *got, int line)
{
    checks++;
    if (!ok)
    {
        failures++;
        printf("FAILED line %d %s\n", line, got);
    }
}

#define CHECK(cond) check((cond) ? 1 : 0, "", __LINE__)
#define CHECK_TEXT(buf, expected) check(strcmp((buf), (expected)) == 0, (buf), __LINE__)

static bool count_entry(const char *key, const char *value, void *user_data)
{
    int *seen = (int *)user_data;

    if (strcmp(key, "only_in_english") == 0 && strcmp(value, "Fallback works") == 0)
    {
        seen[1]++;
    }
    seen[0]++;
    return true;
}

int main(void)
{
    static const char DE[] = "greeting=Hallo, {name}!\n";
    char text[48];
    int seen[2] = {0, 0};

    console_open();
    ci18n_init();

    CHECK(ci18n_use_compiled("en", &ci18n_compiled_en));
    CHECK(ci18n_use_compiled("ru", &ci18n_compiled_ru));
    CHECK(ci18n_set_current("ru"));
    CHECK(ci18n_set_fallback("en"));

    /* A value, and the length it reports. */
    CHECK(ci18n_get_copy(CI18N_KEY(greeting), text, sizeof(text)) == 21);
    CHECK_TEXT(text, "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82, {name}!");

    /* Cut to the buffer, with the full length still reported. */
    CHECK(ci18n_get_copy("greeting", text, 5) == 21);
    CHECK_TEXT(text, "\xd0\x9f\xd1\x80");

    /* Missing everywhere, and missing here but present in the fallback. */
    CHECK(ci18n_get_copy("nowhere", text, sizeof(text)) == 0);
    CHECK_TEXT(text, "");
    CHECK(ci18n_get_copy("only_in_english", text, sizeof(text)) == 14);
    CHECK_TEXT(text, "Fallback works");
    CHECK(ci18n_has("greeting"));
    CHECK(!ci18n_has("nowhere"));

    /* An escape, decoded when the catalogue was compiled. */
    CHECK(ci18n_get_copy("escaped", text, sizeof(text)) == 17);
    CHECK_TEXT(text, "Line one\nLine two");

    /* Plural forms by Russian rules. */
    ci18n_plural_copy("files", 1, text, sizeof(text));
    CHECK_TEXT(text, "{count} \xd1\x84\xd0\xb0\xd0\xb9\xd0\xbb");
    ci18n_plural_copy("files", 3, text, sizeof(text));
    CHECK_TEXT(text, "{count} \xd1\x84\xd0\xb0\xd0\xb9\xd0\xbb\xd0\xb0");
    ci18n_plural_copy("files", 25, text, sizeof(text));
    CHECK_TEXT(text, "{count} \xd1\x84\xd0\xb0\xd0\xb9\xd0\xbb\xd0\xbe\xd0\xb2");
    CHECK(ci18n_plural_copy("nowhere", 1, text, sizeof(text)) == 0);

#ifndef CI18N_NO_ORDINALS
    /* Ordinals by English rules. */
    CHECK(ci18n_set_current("en"));
    ci18n_ordinal_copy("place", 1, text, sizeof(text));
    CHECK_TEXT(text, "{count}st");
    ci18n_ordinal_copy("place", 22, text, sizeof(text));
    CHECK_TEXT(text, "{count}nd");
    ci18n_ordinal_copy("place", 13, text, sizeof(text));
    CHECK_TEXT(text, "{count}th");
    CHECK(ci18n_set_current("ru"));
#endif

#ifndef CI18N_NO_FORMAT
    /* The formatter reads its template out of flash too. */
    ci18n_format(text, sizeof(text), "greeting", "name", "\xd0\x90\xd0\xbd\xd0\xbd\xd0\xb0",
                 NULL);
    CHECK_TEXT(text, "\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82, "
                     "\xd0\x90\xd0\xbd\xd0\xbd\xd0\xb0!");
    ci18n_format_plural(text, sizeof(text), "files", 3, NULL);
    CHECK_TEXT(text, "3 \xd1\x84\xd0\xb0\xd0\xb9\xd0\xbb\xd0\xb0");
#endif

    /* Every entry, each key and value copied out for the callback. */
    CHECK(ci18n_foreach("en", count_entry, seen) == 9);
    CHECK(seen[0] == 9 && seen[1] == 1);

    /* A language loaded at run time sits beside the compiled ones. */
    CHECK(ci18n_load_from_buffer("de", DE, strlen(DE)));
    CHECK(ci18n_set_current("de"));
    ci18n_get_copy("greeting", text, sizeof(text));
    CHECK_TEXT(text, "Hallo, {name}!");
    ci18n_get_copy("only_in_english", text, sizeof(text));
    CHECK_TEXT(text, "Fallback works");

    /* Compiled languages cannot be changed. */
    CHECK(!ci18n_set("en", "greeting", "x"));
    CHECK(ci18n_last_error() == CI18N_ERR_READ_ONLY);

    ci18n_free();

    printf("%d checks\nFailed:  %d\n", checks, failures);
    console_exit(failures == 0 ? 0 : 1);
}
