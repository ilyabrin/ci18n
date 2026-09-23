/*
 * The smallest useful program, built with CI18N_MINIMAL: translations from
 * a buffer, a lookup and a plural. CI builds it for a Cortex-M4 and reports
 * its size, so the minimal build has a number that stays true.
 *
 *   cc -DCI18N_MINIMAL -Iinclude tests/minimal.c
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stdio.h>
#include <string.h>

static const char RU[] =
    "greeting=\xd0\x9f\xd1\x80\xd0\xb8\xd0\xb2\xd0\xb5\xd1\x82\n"
    "files[one]=%ld file\n"
    "files[few]=%ld files, few\n"
    "files[many]=%ld files, many\n";

int main(void)
{
    ci18n_init();
    ci18n_load_from_buffer("ru", RU, strlen(RU));
    ci18n_set_current("ru");

    puts(ci18n_get_or_key("greeting"));
    printf(ci18n_plural_or_key("files", 3), 3L);
    putchar('\n');

    ci18n_free();
    return 0;
}
