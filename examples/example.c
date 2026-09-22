/*
 * ci18n usage example.
 *
 * Build and run from the repository root so the relative translation paths
 * resolve:
 *
 *   gcc -Wall -Wextra -std=c99 -I./include -o example examples/example.c
 *   ./example
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    /* Always init first. Every other call returns false until you do. */
    if (!ci18n_init()) {
        fprintf(stderr, "Failed to initialize ci18n\n");
        return 1;
    }

    /* Load translations from files. A loader returns false only when it could
     * not read the source at all, so check the error for the reason. */
    if (!ci18n_load_language("en", "translations/en.txt")) {
        fprintf(stderr, "en: %s\n", ci18n_error_string(ci18n_last_error()));
    }

    ci18n_load_language("ru", "translations/ru.txt");
    ci18n_load_language("es", "translations/es.txt");

    /* Reading the file says nothing about its contents, so ask what the load
     * actually did with them. */
    {
        const ci18n_load_stats_t *st = ci18n_last_load_stats();

        if (st->lines_malformed > 0) {
            fprintf(stderr, "es: %u bad lines, first at line %u\n",
                    (unsigned int)st->lines_malformed,
                    (unsigned int)st->first_malformed_line);
        }
    }

    /* Or from a buffer, handy for translations baked into the binary. */
    const char *fr =
        "greeting=Bonjour\n"
        "farewell=Au revoir\n"
        "welcome=Bienvenue";
    ci18n_load_from_buffer("fr", fr, strlen(fr));

    /* Pick the active language, plus a fallback for keys it is missing. */
    ci18n_set_current("en");
    ci18n_set_fallback("en");

    printf("=== English ===\n");
    printf("%s\n", ci18n_get("greeting"));
    printf("%s\n", ci18n_get("farewell"));

    /* Switching language affects every later ci18n_get(). */
    ci18n_set_current("ru");
    printf("\n=== Russian ===\n");
    printf("%s\n", ci18n_get("greeting"));
    printf("%s\n", ci18n_get("farewell"));

    /* ci18n_get() returns NULL for an unknown key, ci18n_get_or_key()
     * returns the key itself, which is usually what you want in UI code. */
    printf("\n=== Missing key ===\n");
    printf("%s\n", ci18n_get_or_key("nonexistent_key"));

    printf("\n=== Has key ===\n");
    printf("Has 'greeting': %s\n", ci18n_has("greeting") ? "yes" : "no");
    printf("Has 'missing': %s\n", ci18n_has("missing") ? "yes" : "no");

    /* Entries can also be added at runtime. */
    ci18n_set("en", "dynamic_key", "This was added at runtime!");
    printf("\n=== Dynamic ===\n");
    printf("%s\n", ci18n_get_or_key("dynamic_key"));

    /* Plurals. The count decides which form comes back, and the rules are per
     * language: English needs two forms, Russian needs three, and 11 and 21
     * are where a rule that just checks for 1 gets Russian wrong. */
    printf("\n=== Plurals ===\n");
    {
        static const long counts[] = {1, 2, 5, 11, 21};
        static const char *languages[] = {"en", "ru"};
        size_t li;
        size_t ci;

        for (li = 0; li < sizeof(languages) / sizeof(languages[0]); li++) {
            ci18n_set_current(languages[li]);
            printf("  %s:", languages[li]);

            for (ci = 0; ci < sizeof(counts) / sizeof(counts[0]); ci++) {
                printf("   ");
                printf(ci18n_plural_or_key("files", counts[ci]), (int)counts[ci]);
            }

            printf("\n");
        }
    }

    /* Locale detection. Loading plain "ru" is enough to honour a user whose
     * environment says ru-RU, because the chain drops subtags as it goes. */
    printf("\n=== Locale ===\n");
    {
        char locale[CI18N_MAX_CODE_LENGTH];

        if (ci18n_detect_locale(locale, sizeof(locale)) > 0) {
            printf("  environment reports: %s\n", locale);
        } else {
            printf("  environment reports nothing usable\n");
        }

        if (ci18n_set_current_best(NULL)) {
            printf("  selected from it: %s\n", ci18n_get_current());
        } else {
            printf("  nothing loaded matches it, keeping %s\n", ci18n_get_current());
        }

        printf("  ru-RU resolves to: %s\n",
               ci18n_set_current_best("ru-RU") ? ci18n_get_current() : "nothing");
    }

    /* You own the buffer. Sizing it to CI18N_MAX_LANGUAGES means the return
     * value can never exceed it, so no clamping is needed here. */
    const char *codes[CI18N_MAX_LANGUAGES];
    size_t total = ci18n_get_languages(codes, CI18N_MAX_LANGUAGES);
    printf("\n=== Available languages (%u) ===\n", (unsigned int)total);
    for (size_t i = 0; i < total; i++) {
        printf("  - %s\n", codes[i]);
    }

    /* Releases every language buffer and resets the context. */
    ci18n_free();

    return 0;
}
