/*
 * Must not compile on AVR: ci18n_get() returns a pointer, and there the
 * translation it points to can be in flash. tests/avr/run.sh checks that
 * the build fails, and fails for this reason.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stdio.h>

int main(void)
{
    ci18n_init();
    puts(ci18n_get("greeting"));
    return 0;
}
