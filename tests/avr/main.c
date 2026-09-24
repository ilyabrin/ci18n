/*
 * The unit tests on an 8-bit AVR, where int and size_t are 16 bits wide.
 *
 * An ATmega2560 has 8 KB of RAM, and avr-gcc copies every string literal
 * into RAM at startup, so the suite runs in shards (see TEST_SHARDS in
 * tests/test_ci18n.c). The suite reads translations through pointers, so it
 * runs with CI18N_NO_COMPILED; tests/avr/compiled.c covers the catalogues
 * that live in flash. tests/avr/run.sh builds and runs both:
 *
 *   tests/avr/run.sh
 *
 * SPDX-License-Identifier: MIT
 */

#include <avr/pgmspace.h>
#include <string.h>

#include "console.h"

/* The CLDR tables, 25 KB of plural samples and 14 KB of formatted numbers,
 * are more than the RAM holds, so they stay in flash and are read a row at
 * a time. */
#define CLDR_SAMPLES_STORAGE PROGMEM
#define CLDR_SAMPLE_READ(dst, src) memcpy_P((dst), (src), sizeof(*(dst)))
#define CLDR_NUMBERS_STORAGE PROGMEM
#define CLDR_NUMBER_READ(dst, src) memcpy_P((dst), (src), sizeof(dst))

#define main test_main
#include "../test_ci18n.c"
#undef main

int main(void)
{
    console_open();
    console_exit(test_main());
}
