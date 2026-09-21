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

    /* Load translations from files. */
    ci18n_load_language("en", "translations/en.txt");
    ci18n_load_language("ru", "translations/ru.txt");
    ci18n_load_language("es", "translations/es.txt");

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

    size_t count;
    const char **langs = ci18n_get_languages(&count);
    printf("\n=== Available languages (%u) ===\n", (unsigned int)count);
    for (size_t i = 0; i < count; i++) {
        printf("  - %s\n", langs[i]);
    }

    /* Releases every language buffer and resets the context. */
    ci18n_free();

    return 0;
}
