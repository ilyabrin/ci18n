/*
 * Fuzz target for the translation parser.
 *
 *   make fuzz          build with clang and libFuzzer
 *   make fuzz-run      build, then fuzz for CI18N_FUZZ_SECONDS seconds
 *
 * ci18n_load_from_buffer() takes whatever bytes a program hands it, which for
 * a translation file is often not something the program wrote. Everything the
 * file loader does to a line, the buffer loader does too, so fuzzing this one
 * entry point covers the parser without touching the filesystem on every run.
 *
 * Replaying one input without libFuzzer:
 *
 *   make fuzz-replay INPUT=tests/fuzz_corpus/crlf
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * One round trip: load the bytes, then use what came out of them.
 *
 * Looking things up afterwards matters as much as the parse. A key that was
 * stored truncated, or a value the trimmer walked off the end of, only shows
 * up when something reads it back.
 */

/*
 * Writes the argument back out, following snprintf so the library's own
 * accounting is exercised with a result that can be longer than the buffer.
 */
static size_t fuzz_echo(char *out, size_t capacity, const char *value,
                        const char *arg, void *user_data)
{
    size_t len = strlen(value) + strlen(arg);
    size_t written = 0;
    const char *parts[2];
    size_t p;

    (void)user_data;

    parts[0] = value;
    parts[1] = arg;

    for (p = 0; p < 2; p++)
    {
        size_t i;
        for (i = 0; parts[p][i] != '\0'; i++)
        {
            if (capacity > 0 && written < capacity - 1)
            {
                out[written++] = parts[p][i];
            }
        }
    }

    if (capacity > 0)
    {
        out[written] = '\0';
    }

    return len;
}

static void fuzz_one(const uint8_t *data, size_t size)
{
    const ci18n_load_stats_t *stats;
    const char *codes[4];
    size_t total;
    size_t i;

    if (!ci18n_init())
    {
        return;
    }

    /* Registered so that a {name:echo,...} placeholder in the fuzzed input
     * reaches the formatter path rather than only the unknown-formatter one.
     * The formatter is deliberately dull: what is under test is the library's
     * buffer arithmetic around it, not the formatting. */
    ci18n_set_formatter("echo", fuzz_echo, NULL);

    ci18n_load_from_buffer("fz", (const char *)data, size);

    /* The stats pointer is always valid, and the counters have to stay
     * consistent with each other whatever the input was. */
    stats = ci18n_last_load_stats();
    if (stats->entries_loaded + stats->lines_skipped + stats->lines_malformed !=
        stats->lines_read)
    {
        /* Unreachable unless the accounting is wrong, and worth a crash in a
         * fuzz build so it cannot pass silently. */
        abort();
    }

    if (stats->lines_malformed == 0 && stats->first_malformed_line != 0)
    {
        abort();
    }

    if (ci18n_count("fz") > CI18N_MAX_KEYS_PER_LANGUAGE)
    {
        abort();
    }

    ci18n_set_current("fz");

    /* Read every key back through the public API, using the bytes as keys too
     * so lookups see the same hostile input. */
    ci18n_has("");
    ci18n_get("");
    ci18n_get_or_key("missing");

    if (size > 0)
    {
        char key[CI18N_MAX_KEY_LENGTH];
        size_t len = size < sizeof(key) - 1 ? size : sizeof(key) - 1;

        memcpy(key, data, len);
        key[len] = '\0';

        ci18n_get(key);
        ci18n_has(key);
        ci18n_get_or_key(key);
        ci18n_set("fz", key, "v");
        ci18n_remove("fz", key);
    }

    total = ci18n_get_languages(codes, sizeof(codes) / sizeof(codes[0]));
    for (i = 0; i < total && i < sizeof(codes) / sizeof(codes[0]); i++)
    {
        ci18n_count(codes[i]);
    }

    /* The formatter parses the stored values, which came from this input, so
     * it sees arbitrary bytes too. Braces, half-written placeholders and
     * names that match nothing all arrive here rather than in a test. */
    {
        char rendered[512];
        const char *names[3];
        size_t k;

        names[0] = "name";
        names[1] = "count";
        names[2] = size > 0 ? (const char *)data : "x";

        for (k = 0; k < sizeof(names) / sizeof(names[0]); k++)
        {
            char key[CI18N_MAX_KEY_LENGTH];
            size_t len = size < sizeof(key) - 1 ? size : sizeof(key) - 1;
            size_t needed;

            memcpy(key, data, len);
            key[len] = '\0';

            needed = ci18n_format(rendered, sizeof(rendered), key,
                                  names[k], "value", NULL);

            /* Truncation is allowed; writing past the buffer is not, and the
             * result has to be terminated whatever happened. */
            if (needed > 0 && strlen(rendered) >= sizeof(rendered))
            {
                abort();
            }

            /* Measuring must agree with what a real write reports. */
            if (ci18n_format(NULL, 0, key, names[k], "value", NULL) != needed)
            {
                abort();
            }

            ci18n_format_plural(rendered, sizeof(rendered), key,
                                (long)size, names[k], "value", NULL);
        }
    }

    /* A second load into the same language exercises the merge path, where
     * the entry array is reallocated under pointers taken earlier. */
    ci18n_load_from_buffer("fz", (const char *)data, size);

    ci18n_error_string(ci18n_last_error());

    ci18n_free();
}

#ifdef CI18N_FUZZ_REPLAY

/* Standalone replay, so a crashing input can be re-run under a debugger or a
 * sanitizer without a libFuzzer build. */
int main(int argc, char **argv)
{
    static uint8_t buffer[1 << 20];
    FILE *f;
    size_t size;

    if (argc < 2)
    {
        fprintf(stderr, "usage: %s <input-file>\n", argv[0]);
        return 2;
    }

    f = fopen(argv[1], "rb");
    if (!f)
    {
        fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }

    size = fread(buffer, 1, sizeof(buffer), f);
    fclose(f);

    fuzz_one(buffer, size);
    printf("replayed %s, %u bytes, no crash\n", argv[1], (unsigned int)size);
    return 0;
}

#else

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    fuzz_one(data, size);
    return 0;
}

#endif
