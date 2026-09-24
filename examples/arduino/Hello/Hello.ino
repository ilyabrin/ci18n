/*
 * Hello: two languages compiled into flash, printed to the serial monitor.
 * Switches language every second and counts files, so the Russian plural
 * forms show up: 1 файл, 2 файла, 5 файлов.
 *
 * en.h and ru.h are generated from en.txt and ru.txt. After editing a .txt,
 * regenerate its header on your computer, from the root of the library:
 *
 *   cc -Iinclude -o ci18n_compile tools/ci18n_compile.c
 *   ./ci18n_compile -o examples/arduino/Hello/en.h en examples/arduino/Hello/en.txt
 *
 * Open the serial monitor at 9600 baud.
 *
 * SPDX-License-Identifier: MIT
 */

#include <ci18n.h>

#include "en.h"
#include "ru.h"

static long files = 0;

void setup()
{
    Serial.begin(9600);

    ci18n_init();
    ci18n_use_compiled("en", &ci18n_compiled_en);
    ci18n_use_compiled("ru", &ci18n_compiled_ru);
    ci18n_set_fallback("en");
}

void loop()
{
    char line[48];

    ci18n_set_current(files % 2 ? "ru" : "en");

    /* On AVR a translation stays in flash until it is copied out, so the
     * functions that fill a buffer are the way to read one. */
    ci18n_get_copy(CI18N_KEY(title), line, sizeof(line));
    Serial.println(line);

    ci18n_format(line, sizeof(line), CI18N_KEY(greeting), "name", "Arduino", NULL);
    Serial.println(line);

    ci18n_format_plural(line, sizeof(line), CI18N_KEY(files), files, NULL);
    Serial.println(line);

    files++;
    delay(1000);
}
