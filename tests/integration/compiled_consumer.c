/*
 * Uses a catalogue compiled by ci18n_compile_translations() from an
 * installed ci18n, which exercises the installed generator source and CMake
 * module as well as the header.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"
#include "hello.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    const char *text;

    ci18n_init();
    ci18n_use_compiled("en", &ci18n_compiled_hello);
    ci18n_set_current("en");
    text = ci18n_get(CI18N_KEY(greeting));
    puts(text ? text : "(missing)");
    ci18n_free();
    return (text && strcmp(text, "Hello from a compiled catalogue") == 0) ? 0 : 1;
}
