/* SPDX-License-Identifier: MIT */
/*
 * How fast ci18n is, and how much memory it takes.
 *
 *   make bench
 *
 * Prints a Markdown table you can paste as is. Every number is the median of
 * several timed rounds, each long enough that the clock's resolution does not
 * matter. Nothing here is tuned for the benchmark: it uses the public API the
 * way an application would, with the default build except for one line below.
 */

#define _CRT_SECURE_NO_WARNINGS /* fopen() in MSVC */
#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L /* clock_gettime() under -std=c99 */
#endif

/* The default cap is 1024 keys per language. The 10k catalogue needs more,
 * and raising it is exactly what an application that big would do. */
#define CI18N_MAX_KEYS_PER_LANGUAGE 16384
#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

/* ------------------------------------------------------------------ timing */

static double now_ns(void)
{
#if defined(_WIN32)
    static LARGE_INTEGER freq;
    LARGE_INTEGER t;
    if (freq.QuadPart == 0)
    {
        QueryPerformanceFrequency(&freq);
    }
    QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1e9 / (double)freq.QuadPart;
#else
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
#endif
}

/* Written by every operation, so the compiler cannot drop the work. */
static volatile size_t sink;

typedef void (*bench_fn)(size_t iterations);

#define ROUNDS 7
#define ROUND_NS 100e6 /* 100 ms per round */

static int compare_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

/* Nanoseconds per call: grow the iteration count until one round takes
 * ROUND_NS, then take the median of ROUNDS rounds of that size. */
static double measure(bench_fn fn)
{
    double samples[ROUNDS];
    size_t iterations = 1;
    int i;

    for (;;)
    {
        double start = now_ns();
        fn(iterations);
        if (now_ns() - start >= ROUND_NS / 4 || iterations >= ((size_t)1 << 30))
        {
            break;
        }
        iterations *= 2;
    }
    iterations *= 4;

    for (i = 0; i < ROUNDS; i++)
    {
        double start = now_ns();
        fn(iterations);
        samples[i] = (now_ns() - start) / (double)iterations;
    }
    qsort(samples, ROUNDS, sizeof(samples[0]), compare_double);
    return samples[ROUNDS / 2];
}

static void row(const char *what, double ns)
{
    if (ns >= 1e6)
    {
        printf("| %-44s | %10.2f ms |\n", what, ns / 1e6);
    }
    else if (ns >= 1e3)
    {
        printf("| %-44s | %10.2f us |\n", what, ns / 1e3);
    }
    else
    {
        printf("| %-44s | %10.1f ns |\n", what, ns);
    }
}

/* ------------------------------------------------------------ catalogues */

#define SMALL 1000
#define LARGE 10000
#define FALLBACK_KEYS 256
#define VISIT_MASK 1023 /* lookups walk a shuffled list of 1024 keys */

static char keys[LARGE][32];
static char missing[VISIT_MASK + 1][32];
static char fallback_only[FALLBACK_KEYS][32];
static unsigned visit[VISIT_MASK + 1];

/* A typical UI string: Cyrillic, one placeholder, about 50 bytes. */
static const char *const VALUE = "Элемент {n} сохранён в папку";

static unsigned long lcg_state = 12345;

static unsigned lcg(void)
{
    lcg_state = lcg_state * 1103515245UL + 12345UL;
    return (unsigned)((lcg_state >> 16) & 0x7fff) * 32768U +
           (unsigned)((lcg_state >> 8) & 0x7fff);
}

/* The text of a catalogue file with `count` keys, as a loader would read it. */
static char *make_catalogue(size_t count, size_t *length)
{
    size_t capacity = count * 96 + 64, used = 0, i;
    char *text = (char *)malloc(capacity);
    if (text == NULL)
    {
        exit(1);
    }
    used += (size_t)snprintf(text, capacity, "# benchmark catalogue\n");
    for (i = 0; i < count; i++)
    {
        used += (size_t)snprintf(text + used, capacity - used, "%s=%s\n", keys[i], VALUE);
    }
    *length = used;
    return text;
}

/* Bytes the library itself asked for: arenas, entry arrays, hash buckets and
 * the catalogue struct. Allocator overhead comes on top, typically 8 to 16
 * bytes per block, and there are only three blocks per language. */
static size_t catalogue_bytes(ci18n_t *catalog)
{
    size_t total = sizeof(*catalog), i;
    for (i = 0; i < catalog->language_count; i++)
    {
        const ci18n_language_t *lang = &catalog->languages[i];
        total += lang->strings.capacity;
        total += lang->capacity * sizeof(ci18n_entry_t);
        total += lang->bucket_count * sizeof(uint32_t);
    }
    return total;
}

/* ------------------------------------------------------------ operations */

static ci18n_t *cat;
static const char *load_text;
static size_t load_length;

static void op_get_hit(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += (size_t)ci18n_get_in(cat, keys[visit[i & VISIT_MASK]]);
    }
}

static void op_get_miss(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += (size_t)ci18n_get_in(cat, missing[i & VISIT_MASK]);
    }
}

static void op_get_fallback(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += (size_t)ci18n_get_in(cat, fallback_only[i % FALLBACK_KEYS]);
    }
}

static void op_format(size_t n)
{
    char out[128];
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += ci18n_format_in(cat, out, sizeof(out), "greeting",
                                "name", "Анна", "count", "12", NULL);
    }
}

static void op_format_plural(size_t n)
{
    char out[128];
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += ci18n_format_plural_in(cat, out, sizeof(out), "files", (long)(i & 1023), NULL);
    }
}

static void op_format_ordinal(size_t n)
{
    char out[128];
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += ci18n_format_ordinal_in(cat, out, sizeof(out), "place", (long)(i & 1023), NULL);
    }
}

static void op_format_formatter(size_t n)
{
    char out[128];
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += ci18n_format_in(cat, out, sizeof(out), "saved", "size", "1536000", NULL);
    }
}

static void op_plural_category(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += (size_t)ci18n_plural_category("ru", (long)(i & 1023));
    }
}

static void op_utf8_length(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        sink += ci18n_utf8_length(VALUE);
    }
}

static void op_load_buffer(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        ci18n_t *fresh = ci18n_create();
        sink += ci18n_load_from_buffer_in(fresh, "ru", load_text, load_length);
        ci18n_destroy(fresh);
    }
}

static const char *const CATALOGUE_FILE = "bench_catalogue.txt";

static void op_load_file(size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        ci18n_t *fresh = ci18n_create();
        sink += ci18n_load_language_in(fresh, "ru", CATALOGUE_FILE);
        ci18n_destroy(fresh);
    }
}

/* A byte-size formatter of the kind an application would register. */
static size_t format_bytes(char *out, size_t capacity, const char *value,
                           const char *arg, void *user_data)
{
    double bytes = strtod(value, NULL);
    int written;
    (void)arg;
    (void)user_data;
    if (bytes >= 1048576.0)
    {
        written = snprintf(out, capacity, "%.1f МБ", bytes / 1048576.0);
    }
    else
    {
        written = snprintf(out, capacity, "%.0f КБ", bytes / 1024.0);
    }
    return written < 0 ? 0 : (size_t)written;
}

/* ------------------------------------------------------------------- main */

static void print_toolchain(void)
{
#if defined(__clang__)
    printf("Compiler: clang %s", __clang_version__);
#elif defined(__GNUC__)
    printf("Compiler: gcc %s", __VERSION__);
#elif defined(_MSC_VER)
    printf("Compiler: MSVC %d", _MSC_VER);
#else
    printf("Compiler: unknown");
#endif
    printf(", %u-bit, ci18n %s\n\n", (unsigned)(sizeof(void *) * 8), CI18N_VERSION_STRING);
}

static bool write_file(const char *path, const char *text, size_t length)
{
    FILE *f = fopen(path, "wb");
    bool ok;
    if (f == NULL)
    {
        return false;
    }
    ok = fwrite(text, 1, length, f) == length;
    return fclose(f) == 0 && ok;
}

int main(void)
{
    size_t i, payload;
    char *small_text, *large_text;
    size_t small_length, large_length;

    for (i = 0; i < LARGE; i++)
    {
        snprintf(keys[i], sizeof(keys[i]), "screen.item_%05u", (unsigned)i);
    }
    for (i = 0; i <= VISIT_MASK; i++)
    {
        snprintf(missing[i], sizeof(missing[i]), "screen.absent_%05u", (unsigned)i);
        visit[i] = lcg() % SMALL;
    }
    for (i = 0; i < FALLBACK_KEYS; i++)
    {
        snprintf(fallback_only[i], sizeof(fallback_only[i]), "legacy.item_%05u", (unsigned)i);
    }
    small_text = make_catalogue(SMALL, &small_length);
    large_text = make_catalogue(LARGE, &large_length);

    print_toolchain();

    /* Lookups against a 1000-key Russian catalogue, English as fallback. */
    cat = ci18n_create();
    ci18n_load_from_buffer_in(cat, "ru", small_text, small_length);
    for (i = 0; i < FALLBACK_KEYS; i++)
    {
        ci18n_set_in(cat, "en", fallback_only[i], "Saved");
    }
    ci18n_set_in(cat, "ru", "greeting", "Привет, {name}! У вас {count} новых сообщений.");
    ci18n_set_in(cat, "ru", "files[one]", "{count} файл");
    ci18n_set_in(cat, "ru", "files[few]", "{count} файла");
    ci18n_set_in(cat, "ru", "files[many]", "{count} файлов");
    ci18n_set_in(cat, "ru", "files[other]", "{count} файла");
    ci18n_set_in(cat, "ru", "place[other]", "{count}-е место");
    ci18n_set_in(cat, "ru", "saved", "Сохранено {size:bytes}");
    ci18n_set_formatter_in(cat, "bytes", format_bytes, NULL);
    ci18n_set_current_in(cat, "ru");
    ci18n_set_fallback_in(cat, "en");

    printf("| Operation                                    |       Time    |\n");
    printf("|----------------------------------------------|---------------|\n");
    row("`get`, key found", measure(op_get_hit));
    row("`get`, key missing everywhere", measure(op_get_miss));
    row("`get`, found in the fallback language", measure(op_get_fallback));
    row("`format`, two placeholders", measure(op_format));
    row("`format`, value through a formatter", measure(op_format_formatter));
    row("`format_plural`", measure(op_format_plural));
    row("`format_ordinal`", measure(op_format_ordinal));
    row("`plural_category` alone", measure(op_plural_category));
    row("`utf8_length`, 28-character string", measure(op_utf8_length));

    load_text = small_text;
    load_length = small_length;
    row("load 1 000 keys from a buffer", measure(op_load_buffer));
    load_text = large_text;
    load_length = large_length;
    row("load 10 000 keys from a buffer", measure(op_load_buffer));
    if (write_file(CATALOGUE_FILE, large_text, large_length))
    {
        row("load 10 000 keys from a file", measure(op_load_file));
        remove(CATALOGUE_FILE);
    }
    ci18n_destroy(cat);

    /* Memory, one language at a time. */
    printf("\n| Catalogue               | Text on disk | In memory | Per key |\n");
    printf("|-------------------------|--------------|-----------|---------|\n");
    {
        size_t sizes[2];
        const char *texts[2];
        size_t lengths[2];
        int s;
        sizes[0] = SMALL;
        sizes[1] = LARGE;
        texts[0] = small_text;
        texts[1] = large_text;
        lengths[0] = small_length;
        lengths[1] = large_length;
        for (s = 0; s < 2; s++)
        {
            size_t bytes;
            cat = ci18n_create();
            ci18n_load_from_buffer_in(cat, "ru", texts[s], lengths[s]);
            bytes = catalogue_bytes(cat);
            payload = lengths[s];
            printf("| %6u keys, 1 language | %9.1f KB | %6.1f KB | %5.0f B |\n",
                   (unsigned)sizes[s], (double)payload / 1024.0, (double)bytes / 1024.0,
                   (double)(bytes - sizeof(*cat)) / (double)sizes[s]);
            ci18n_destroy(cat);
        }
    }
    printf("\nEmpty catalogue: %u bytes, no heap.\n", (unsigned)sizeof(ci18n_t));

    free(small_text);
    free(large_text);
    return sink == 0 ? 1 : 0;
}
