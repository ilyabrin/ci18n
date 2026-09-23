/* SPDX-License-Identifier: MIT */
/*
 * The same lookups through GNU gettext, for a point of reference.
 *
 *   make bench-gettext
 *
 * Linux with glibc only. Needs msgfmt (the gettext package) and the
 * ru_RU.UTF-8 locale, since gettext translates nothing in the C locale:
 *
 *   sudo apt-get install gettext locales && sudo locale-gen ru_RU.UTF-8
 *
 * The catalogue is the one bench.c uses, compiled to a .mo file.
 */

#define _POSIX_C_SOURCE 200809L
#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define KEYS 1000
#define ROUNDS 7
#define ITERATIONS 4000000

static char keys[KEYS][32];
static char missing[KEYS][32];
static volatile size_t sink;

static double now_ns(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

typedef void (*bench_fn)(size_t iterations);

static double measure(bench_fn fn)
{
    double samples[ROUNDS];
    int i;
    fn(ITERATIONS / 10); /* warm up: gettext maps the file on first use */
    for (i = 0; i < ROUNDS; i++)
    {
        double start = now_ns();
        fn(ITERATIONS);
        samples[i] = (now_ns() - start) / ITERATIONS;
    }
    qsort(samples, ROUNDS, sizeof(samples[0]), compare_double);
    return samples[ROUNDS / 2];
}

static void op_hit(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += (size_t)gettext(keys[(i * 7919) % KEYS]);
    }
}

static void op_miss(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += (size_t)gettext(missing[i % KEYS]);
    }
}

static void op_ngettext(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += (size_t)ngettext("%lu file", "%lu files", (unsigned long)(i & 1023));
    }
}

int main(void)
{
    FILE *po;
    int i;

    mkdir("bench_locale", 0755);
    mkdir("bench_locale/ru", 0755);
    mkdir("bench_locale/ru/LC_MESSAGES", 0755);

    po = fopen("bench_locale/bench.po", "w");
    if (po == NULL)
    {
        perror("bench_locale/bench.po");
        return 1;
    }
    fprintf(po, "msgid \"\"\nmsgstr \"\"\n"
                "\"Content-Type: text/plain; charset=UTF-8\\n\"\n"
                "\"Plural-Forms: nplurals=3; plural=(n%%10==1 && n%%100!=11 ? 0 : "
                "n%%10>=2 && n%%10<=4 && (n%%100<10 || n%%100>=20) ? 1 : 2);\\n\"\n\n");
    for (i = 0; i < KEYS; i++)
    {
        snprintf(keys[i], sizeof(keys[i]), "screen.item_%05d", i);
        snprintf(missing[i], sizeof(missing[i]), "screen.absent_%05d", i);
        fprintf(po, "msgid \"%s\"\nmsgstr \"Элемент {n} сохранён в папку\"\n\n", keys[i]);
    }
    fprintf(po, "msgid \"%%lu file\"\nmsgid_plural \"%%lu files\"\n"
                "msgstr[0] \"%%lu файл\"\nmsgstr[1] \"%%lu файла\"\nmsgstr[2] \"%%lu файлов\"\n");
    fclose(po);

    if (system("msgfmt -o bench_locale/ru/LC_MESSAGES/bench.mo bench_locale/bench.po") != 0)
    {
        fprintf(stderr, "msgfmt failed; is the gettext package installed?\n");
        return 1;
    }
    if (setlocale(LC_ALL, "ru_RU.UTF-8") == NULL)
    {
        fprintf(stderr, "no ru_RU.UTF-8 locale; run: sudo locale-gen ru_RU.UTF-8\n");
        return 1;
    }
    setlocale(LC_NUMERIC, "C"); /* "127.3", not the Russian "127,3" */
    bindtextdomain("bench", "bench_locale");
    textdomain("bench");
    if (strcmp(gettext(keys[0]), keys[0]) == 0)
    {
        fprintf(stderr, "gettext returned the key untranslated; the setup is wrong\n");
        return 1;
    }

    printf("| Operation (glibc gettext, same 1 000 keys) |    Time    |\n");
    printf("|--------------------------------------------|------------|\n");
    printf("| `gettext`, key found                       | %7.1f ns |\n", measure(op_hit));
    printf("| `gettext`, key missing                     | %7.1f ns |\n", measure(op_miss));
    printf("| `ngettext`, form only, no formatting       | %7.1f ns |\n", measure(op_ngettext));

    remove("bench_locale/ru/LC_MESSAGES/bench.mo");
    remove("bench_locale/bench.po");
    rmdir("bench_locale/ru/LC_MESSAGES");
    rmdir("bench_locale/ru");
    rmdir("bench_locale");
    return sink == 0 ? 1 : 0;
}
