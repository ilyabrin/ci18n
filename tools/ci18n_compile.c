/*
 * ci18n_compile: turn a translation file into constant C data.
 *
 *   cc -Iinclude -o ci18n_compile tools/ci18n_compile.c
 *   ./ci18n_compile -o ru.h ru translations/ru.txt
 *
 * The output defines `ci18n_compiled_ru`, which the program hands to
 * ci18n_use_compiled("ru", &ci18n_compiled_ru): no parsing at startup, no
 * heap, and strings that live as long as the program.
 *
 * It also defines a macro per key, so CI18N_KEY(greeting) compiles and
 * CI18N_KEY(greting) does not. --keys-only writes just those, for a program
 * that loads its translations at run time and still wants typos caught. A
 * plural key counts once, by its base: files[one] gives CI18N_KEY(files).
 *
 * Options:
 *   -o FILE       write there instead of to standard output
 *   --keys-only   the key macros alone, no translations
 *   --strict      fail on a key that is not a C identifier, which gets no
 *                 macro, instead of listing it
 *
 * The file is read by the library's own loader, so it means exactly what it
 * would mean loaded at run time. What the loader would drop or cut, a
 * malformed line or a key over the length limit, is an error here, because
 * a build is the cheapest place to find out. Build this tool with the same
 * CI18N_MAX_* settings as the program, if the program changes them.
 *
 * SPDX-License-Identifier: MIT
 */

/* A tool of its own, not a caller's code, so the Windows CRT's deprecation
 * of fopen can be switched off for the whole file. */
#define _CRT_SECURE_NO_WARNINGS

/* The compiled form has no per-language key limit of its own. */
#ifndef CI18N_MAX_KEYS_PER_LANGUAGE
#define CI18N_MAX_KEYS_PER_LANGUAGE 65534
#endif

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
    const char **keys;
    const char **values;
    uint32_t *hashes;
    size_t count;
} entries_t;

static bool collect(const char *key, const char *value, void *user_data)
{
    entries_t *e = (entries_t *)user_data;

    e->keys[e->count] = key;
    e->values[e->count] = value;
    e->hashes[e->count] = ci18n_hash(key, strlen(key));
    e->count++;
    return true;
}

static uint32_t next_power_of_two(size_t n)
{
    uint32_t p = 1;

    while (p < n)
    {
        p <<= 1;
    }
    return p;
}

static bool is_identifier(const char *s)
{
    size_t i;

    for (i = 0; s[i]; i++)
    {
        char c = s[i];
        bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';

        if (!letter && !(i > 0 && c >= '0' && c <= '9'))
        {
            return false;
        }
    }
    return i > 0;
}

/* Buckets are placed largest first, each with the first seed that sends all
 * its keys to free slots. Slots outnumber keys by at least a quarter, so a
 * seed turns up quickly even for the last buckets. */
static bool build_hash(const entries_t *e, uint32_t slot_count, uint32_t seed_count,
                       uint32_t *slots, uint16_t *seeds)
{
    size_t *order = (size_t *)malloc(sizeof(size_t) * (seed_count + 1));
    size_t *size = (size_t *)calloc(seed_count, sizeof(size_t));
    uint32_t *tried = (uint32_t *)malloc(sizeof(uint32_t) * (e->count + 1));
    size_t b;
    size_t i;
    bool ok = true;

    if (!order || !size || !tried)
    {
        free(order);
        free(size);
        free(tried);
        return false;
    }

    for (i = 0; i < slot_count; i++)
    {
        slots[i] = 0xFFFFFFFFu;
    }
    for (i = 0; i < e->count; i++)
    {
        size[e->hashes[i] & (seed_count - 1)]++;
    }
    for (b = 0; b < seed_count; b++)
    {
        size_t j = b;

        /* Insertion sort by size, descending. */
        while (j > 0 && size[order[j - 1]] < size[b])
        {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = b;
        seeds[b] = 0;
    }

    for (b = 0; b < seed_count && ok; b++)
    {
        size_t bucket = order[b];
        uint32_t seed;

        if (size[bucket] == 0)
        {
            break;
        }

        for (seed = 0; seed <= 0xFFFFu; seed++)
        {
            size_t placed = 0;
            bool fits = true;

            for (i = 0; i < e->count && fits; i++)
            {
                uint32_t slot;
                size_t k;

                if ((e->hashes[i] & (seed_count - 1)) != bucket)
                {
                    continue;
                }
                slot = ci18n_compiled_mix(e->hashes[i], seed) & (slot_count - 1);
                if (slots[slot] != 0xFFFFFFFFu)
                {
                    fits = false;
                }
                for (k = 0; k < placed && fits; k++)
                {
                    fits = tried[k] != slot;
                }
                tried[placed++] = slot;
            }

            if (fits)
            {
                size_t n = 0;

                for (i = 0; i < e->count; i++)
                {
                    if ((e->hashes[i] & (seed_count - 1)) == bucket)
                    {
                        slots[tried[n++]] = (e->hashes[i] & 0xFFFF0000u) | (uint32_t)i;
                    }
                }
                seeds[bucket] = (uint16_t)seed;
                break;
            }
        }
        ok = seed <= 0xFFFFu;
    }

    free(order);
    free(size);
    free(tried);
    return ok;
}

/* The part of a key a macro names: "files[one]" gives "files". */
static size_t base_length(const char *key)
{
    const char *open = strchr(key, '[');

    return open ? (size_t)(open - key) : strlen(key);
}

static bool is_identifier_n(const char *s, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++)
    {
        char c = s[i];
        bool letter = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';

        if (!letter && !(i > 0 && c >= '0' && c <= '9'))
        {
            return false;
        }
    }
    return n > 0;
}

/* One macro per distinct key base. Returns how many keys could not have
 * one, having listed them. */
static size_t emit_keys(FILE *out, const entries_t *e)
{
    size_t skipped = 0;
    size_t i;
    size_t j;

    fprintf(out, "/* Keys, for CI18N_KEY(). */\n");
    for (i = 0; i < e->count; i++)
    {
        size_t n = base_length(e->keys[i]);
        bool seen = false;

        for (j = 0; j < i && !seen; j++)
        {
            seen = base_length(e->keys[j]) == n && strncmp(e->keys[j], e->keys[i], n) == 0;
        }
        if (seen)
        {
            continue;
        }
        if (!is_identifier_n(e->keys[i], n))
        {
            fprintf(stderr, "  no CI18N_KEY for \"%.*s\": not a C identifier\n", (int)n,
                    e->keys[i]);
            skipped++;
            continue;
        }
        fprintf(out, "#ifndef ci18n_key_%.*s\n#define ci18n_key_%.*s 1\n#endif\n", (int)n,
                e->keys[i], (int)n, e->keys[i]);
    }
    fprintf(out, "\n");
    return skipped;
}

static void release(entries_t *e, uint32_t *slots, uint16_t *seeds, uint32_t *offsets,
                    unsigned char *strings)
{
    free((void *)e->keys);
    free((void *)e->values);
    free(e->hashes);
    free(slots);
    free(seeds);
    free(offsets);
    free(strings);
    ci18n_free();
}

/* Every byte as an octal character constant. A plain number above 127 would
 * overflow a signed char, and a string literal would run into the length
 * limits compilers set on literals, well below a large catalogue. */
static void emit_bytes(FILE *out, const unsigned char *data, size_t length)
{
    size_t i;

    for (i = 0; i < length; i++)
    {
        fprintf(out, "%s'\\%03o',", (i % 12 == 0) ? "\n    " : " ", data[i]);
    }
}

int main(int argc, char **argv)
{
    const char *out_path = NULL;
    const char *name;
    const char *path;
    const ci18n_load_stats_t *st;
    entries_t e;
    size_t n;
    uint32_t slot_count;
    uint32_t seed_count;
    uint32_t *slots;
    uint16_t *seeds;
    unsigned char *strings;
    uint32_t *offsets;
    size_t used = 0;
    size_t i;
    FILE *out = stdout;
    bool keys_only = false;
    bool strict = false;
    size_t skipped;
    int argi = 1;

    while (argi < argc && argv[argi][0] == '-')
    {
        if (strcmp(argv[argi], "-o") == 0 && argi + 1 < argc)
        {
            out_path = argv[argi + 1];
            argi += 2;
        }
        else if (strcmp(argv[argi], "--keys-only") == 0)
        {
            keys_only = true;
            argi++;
        }
        else if (strcmp(argv[argi], "--strict") == 0)
        {
            strict = true;
            argi++;
        }
        else
        {
            break;
        }
    }
    if (argc - argi != 2 || !is_identifier(argv[argi]))
    {
        fprintf(stderr, "usage: %s [-o OUT.h] [--keys-only] [--strict] NAME FILE\n"
                        "  NAME becomes ci18n_compiled_NAME and must be a C identifier\n",
                argv[0]);
        return 2;
    }
    name = argv[argi];
    path = argv[argi + 1];

    ci18n_init();
    if (!ci18n_load_language("x", path))
    {
        fprintf(stderr, "%s: %s\n", path, ci18n_error_string(ci18n_last_error()));
        return 1;
    }
    st = ci18n_last_load_stats();
    if (st->lines_malformed)
    {
        fprintf(stderr, "%s:%u: no '=' on the line (%u such lines)\n", path,
                (unsigned)st->first_malformed_line, (unsigned)st->lines_malformed);
        return 1;
    }
    if (st->keys_truncated || st->values_truncated || st->lines_truncated)
    {
        fprintf(stderr, "%s: %u keys, %u values and %u lines exceed the CI18N_MAX_* limits\n",
                path, (unsigned)st->keys_truncated, (unsigned)st->values_truncated,
                (unsigned)st->lines_truncated);
        return 1;
    }

    n = ci18n_count("x");
    e.keys = (const char **)malloc(sizeof(char *) * (n + 1));
    e.values = (const char **)malloc(sizeof(char *) * (n + 1));
    e.hashes = (uint32_t *)malloc(sizeof(uint32_t) * (n + 1));
    e.count = 0;
    if (!e.keys || !e.values || !e.hashes)
    {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    ci18n_foreach("x", collect, &e);

    /* Entry indexes are 16 bits in the slot table. */
    if (n > 65534)
    {
        fprintf(stderr, "%s: %lu keys, more than a compiled language holds (65534)\n", path,
                (unsigned long)n);
        return 1;
    }

    slot_count = next_power_of_two(n + n / 4 + 1);
    seed_count = next_power_of_two(n / 2 + 1);
    slots = (uint32_t *)malloc(sizeof(uint32_t) * slot_count);
    seeds = (uint16_t *)malloc(sizeof(uint16_t) * seed_count);
    offsets = (uint32_t *)malloc(sizeof(uint32_t) * 3 * (n + 1));
    strings = NULL;
    if (!slots || !seeds || !offsets || !build_hash(&e, slot_count, seed_count, slots, seeds))
    {
        fprintf(stderr, "%s: could not build the hash table; please report this\n", path);
        return 1;
    }

    for (i = 0; i < n; i++)
    {
        used += strlen(e.keys[i]) + strlen(e.values[i]) + 2;
    }
    strings = (unsigned char *)malloc(used + 1);
    if (!strings)
    {
        fprintf(stderr, "out of memory\n");
        return 1;
    }
    used = 0;
    for (i = 0; i < n; i++)
    {
        size_t k = strlen(e.keys[i]) + 1;
        size_t v = strlen(e.values[i]) + 1;

        offsets[3 * i] = (uint32_t)used;
        memcpy(strings + used, e.keys[i], k);
        used += k;
        offsets[3 * i + 1] = (uint32_t)used;
        offsets[3 * i + 2] = e.hashes[i];
        memcpy(strings + used, e.values[i], v);
        used += v;
    }

    if (out_path && !(out = fopen(out_path, "w")))
    {
        fprintf(stderr, "%s: cannot write\n", out_path);
        return 1;
    }

    fprintf(out, "/* Generated by tools/ci18n_compile from %s. Do not edit. */\n\n", path);
    fprintf(out, "#ifndef CI18N_COMPILED_%s_H\n#define CI18N_COMPILED_%s_H\n\n", name, name);
    fprintf(out, "#include \"ci18n.h\"\n\n");
    skipped = emit_keys(out, &e);
    if (skipped && strict)
    {
        fprintf(stderr, "%s: %lu keys are not C identifiers (--strict)\n", path,
                (unsigned long)skipped);
        if (out != stdout)
        {
            fclose(out);
            remove(out_path);
        }
        return 1;
    }
    if (keys_only)
    {
        fprintf(out, "#endif\n");
        if (out != stdout)
        {
            fclose(out);
        }
        release(&e, slots, seeds, offsets, strings);
        return 0;
    }

    fprintf(out, "static const char ci18n_%s_strings[] = {", name);
    emit_bytes(out, strings, used);
    fprintf(out, "\n    0\n};\n\n");

    fprintf(out, "static const uint32_t ci18n_%s_entries[] = {", name);
    for (i = 0; i < 3 * n; i++)
    {
        fprintf(out, "%s%luu,", (i % 6 == 0) ? "\n    " : " ", (unsigned long)offsets[i]);
    }
    fprintf(out, "\n    0\n};\n\n");

    fprintf(out, "static const uint32_t ci18n_%s_slots[] = {", name);
    for (i = 0; i < slot_count; i++)
    {
        fprintf(out, "%s%luu,", (i % 6 == 0) ? "\n    " : " ", (unsigned long)slots[i]);
    }
    fprintf(out, "\n};\n\n");

    fprintf(out, "static const uint16_t ci18n_%s_seeds[] = {", name);
    for (i = 0; i < seed_count; i++)
    {
        fprintf(out, "%s%u,", (i % 12 == 0) ? "\n    " : " ", seeds[i]);
    }
    fprintf(out, "\n};\n\n");

    fprintf(out, "static const ci18n_compiled_t ci18n_compiled_%s = {\n", name);
    fprintf(out, "    CI18N_COMPILED_FORMAT, %lu, %lu, %lu,\n", (unsigned long)n,
            (unsigned long)(slot_count - 1), (unsigned long)(seed_count - 1));
    fprintf(out, "    ci18n_%s_strings, ci18n_%s_entries, ci18n_%s_slots, ci18n_%s_seeds\n};\n",
            name, name, name, name);
    fprintf(out, "\n#endif\n");

    if (out != stdout)
    {
        fclose(out);
    }
    fprintf(stderr, "%s: %lu entries, %lu bytes of strings, %lu slots, %lu seeds\n", path,
            (unsigned long)n, (unsigned long)used, (unsigned long)slot_count,
            (unsigned long)seed_count);

    release(&e, slots, seeds, offsets, strings);
    return 0;
}
