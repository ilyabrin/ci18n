/*
 * A settings screen on a 20x4 character display, in English, Russian and
 * Arabic.
 *
 * What a small device needs from an i18n library, and how to get it here:
 *
 *   - Limits sized to the product, set before the include.
 *   - Translations compiled into the firmware: no heap, no parsing at boot.
 *   - A language pack received at run time, checked before it is trusted,
 *     living beside the compiled ones.
 *   - Text fitted to a fixed number of columns, by characters, not bytes.
 *   - A string stored in a fixed-size byte slot, cut without splitting a
 *     character.
 *   - Right-to-left layout: the screen mirrors when the language says so.
 *
 * SPDX-License-Identifier: MIT
 */

/* Sized for this product. The context is a fixed struct, so these decide
 * its RAM cost; the program prints it on stderr. */
#define CI18N_MAX_LANGUAGES 4
#define CI18N_MAX_KEYS_PER_LANGUAGE 16
#define CI18N_MAX_KEY_LENGTH 24
#define CI18N_MAX_VALUE_LENGTH 128
#define CI18N_MAX_FORMATTERS 1

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stdio.h>
#include <string.h>

#define COLS 20

/* ------------------------------------------------------------------------
 * Translations in flash
 *
 * locales/en.txt, ru.txt and ar.txt, compiled by tools/ci18n_compile into
 * constant data: no parsing at boot, and no RAM spent on them at all. The
 * build writes these headers; see the README.
 * ------------------------------------------------------------------------ */

#include "en.h"
#include "ru.h"
#include "ar.h"

/* ------------------------------------------------------------------------
 * Fitting text to the display
 * ------------------------------------------------------------------------ */

/*
 * Copies at most `cols` characters of src into dst, ending with "…" when
 * something had to go. Counts code points, which is what a character LCD
 * shows one per cell. A graphical display with wide CJK glyphs or combining
 * marks needs a real width function instead.
 */
static void fit(char *dst, size_t capacity, const char *src, size_t cols)
{
    const char *ellipsis = "\xE2\x80\xA6";
    size_t chars = 0;
    size_t bytes = 0;

    if (ci18n_utf8_length(src) <= cols)
    {
        (void)snprintf(dst, capacity, "%s", src);
        return;
    }
    while (src[bytes] && chars < cols - 1)
    {
        bytes += ci18n_utf8_sequence_length(src + bytes);
        chars++;
    }
    (void)snprintf(dst, capacity, "%.*s%s", (int)bytes, src, ellipsis);
}

/* One display row: `left` against one edge, `right` against the other,
 * spaces between. A right-to-left language swaps the edges. */
static void row(const char *left, const char *right, ci18n_direction_t dir)
{
    char a[COLS * 4 + 1];
    char b[COLS * 4 + 1];
    size_t used;
    size_t gap;

    fit(a, sizeof(a), left, COLS);
    used = ci18n_utf8_length(a);
    fit(b, sizeof(b), right, used < COLS ? COLS - used : 0);
    used += ci18n_utf8_length(b);
    gap = COLS - used;

    if (dir == CI18N_DIR_RTL)
    {
        printf("|%s%*s%s|\n", b, (int)gap, "", a);
    }
    else
    {
        printf("|%s%*s%s|\n", a, (int)gap, "", b);
    }
}

static void border(void)
{
    char line[COLS + 1];

    memset(line, '-', COLS);
    line[COLS] = '\0';
    printf("+%s+\n", line);
}

/* ------------------------------------------------------------------------
 * The screen
 * ------------------------------------------------------------------------ */

static void draw(const char *language)
{
    ci18n_direction_t dir;
    char battery[COLS * 4 + 1];
    char title[COLS * 4 + 1];

    ci18n_set_current(language);
    dir = ci18n_current_direction();

    /* 21 is a good test number: "one" in Russian, "many" in Arabic, and
     * "other" in English. */
    ci18n_format_plural(battery, sizeof(battery), "battery", 21, NULL);

    printf("%s, %s\n", language, ci18n_direction_name(dir));
    border();
    /* The back arrow sits on the leading edge and points toward it. */
    (void)snprintf(title, sizeof(title),
                   dir == CI18N_DIR_RTL ? "%s >" : "< %s", ci18n_get_or_key("title"));
    row(title, "", dir);
    row(ci18n_get_or_key("brightness"), "80%", dir);
    row(battery, "", dir);
    row(ci18n_get_or_key("update"), "", dir);
    border();
}

/* ------------------------------------------------------------------------
 * Language packs and byte slots
 * ------------------------------------------------------------------------ */

/* A pack arrives over the air or the serial port. Bytes from outside are
 * checked before any of it reaches the display. */
static void install_pack(const char *code, const char *pack, size_t len)
{
    if (!ci18n_utf8_valid(pack))
    {
        printf("pack %s: rejected, not valid UTF-8\n", code);
        return;
    }
    ci18n_load_from_buffer(code, pack, len);
    printf("pack %s: %u entries\n", code, (unsigned)ci18n_count(code));
}

/* The owner's name lives in a 16-byte slot in EEPROM. Cutting it at byte
 * 15 could split a Cyrillic letter in two; ci18n_utf8_truncate backs off to
 * the last whole character. */
static void store_owner(const char *name)
{
    char text[CI18N_MAX_VALUE_LENGTH];
    char slot[16];
    size_t len;

    /* Truncate first, then copy. The other way round, the copy itself cuts
     * at byte 15, and a cut string already fits, so there is nothing left
     * for ci18n_utf8_truncate to repair. */
    (void)snprintf(text, sizeof(text), "%s", name);
    len = ci18n_utf8_truncate(text, sizeof(slot) - 1);
    memcpy(slot, text, len + 1);
    printf("owner slot: \"%s\", %u bytes, %s\n", slot, (unsigned)strlen(slot),
           ci18n_utf8_valid(slot) ? "valid" : "BROKEN");
}

int main(void)
{
    /* The last byte, 0xC3, starts a two-byte character that never comes. */
    static const char corrupt_pack[] = "title=Paramètres\nbrightness=Luminosit\xC3";
    static const char fr_pack[] = "title=Paramètres\nbrightness=Luminosité\n";

    if (!ci18n_init())
    {
        return 1;
    }
    fprintf(stderr, "context: %u bytes of RAM\n", (unsigned)sizeof(ci18n_context_t));

    /* Each call records a pointer; nothing is copied. */
    ci18n_use_compiled("en", &ci18n_compiled_en);
    ci18n_use_compiled("ru", &ci18n_compiled_ru);
    ci18n_use_compiled("ar", &ci18n_compiled_ar);
    ci18n_set_fallback("en");

    draw("en");
    draw("ru");
    draw("ar");

    install_pack("fr", corrupt_pack, strlen(corrupt_pack));
    install_pack("fr", fr_pack, strlen(fr_pack));

    /* A pack with two keys still gives a full screen: the rest falls back
     * to English, a compiled language behind a loaded one. */
    draw("fr");

    store_owner("Александра Петровна");

    ci18n_free();
    return 0;
}
