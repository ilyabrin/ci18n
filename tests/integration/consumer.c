/*
 * Integration consumer. Includes ci18n the way a downstream project does,
 * by include path rather than by relative file path, so it only builds if
 * the packaging actually works.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include <ci18n.h>

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *buffer = "greeting=Hello\nbad line\n";
    const ci18n_load_stats_t *stats;

    if (!ci18n_init())
    {
        fprintf(stderr, "ci18n_init failed\n");
        return 1;
    }

    if (!ci18n_load_from_buffer("en", buffer, strlen(buffer)))
    {
        fprintf(stderr, "load failed: %s\n", ci18n_error_string(ci18n_last_error()));
        return 1;
    }

    ci18n_set_current("en");

    if (strcmp(ci18n_get_or_key("greeting"), "Hello") != 0)
    {
        fprintf(stderr, "wrong translation\n");
        return 1;
    }

    /* The diagnostics have to be reachable from a consumer too, not just from
     * inside this repository. */
    stats = ci18n_last_load_stats();
    if (stats->entries_loaded != 1 || stats->lines_malformed != 1)
    {
        fprintf(stderr, "unexpected load stats\n");
        return 1;
    }

    printf("ci18n %s consumed successfully\n", CI18N_VERSION_STRING);
    ci18n_free();
    return 0;
}
