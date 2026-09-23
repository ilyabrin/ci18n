/*
 * Shared context tests, for CI18N_THREAD_SHARED.
 *
 *   make test-shared
 *
 * The other threading mode gives each thread its own context, which is easy
 * to verify: nothing is shared, so nothing can race. This one is the hard
 * case, one context behind a reader-writer lock, and the only way to trust it
 * is to run readers and writers at the same time under ThreadSanitizer.
 *
 * Two things are checked that a single-threaded test cannot reach:
 *
 *   - readers never observe a torn or half-written state while a writer is
 *     loading, reloading and clearing underneath them
 *   - ci18n_last_error() belongs to the calling thread. If it were shared,
 *     a reader missing a key would clobber the writer's error and neither
 *     could trust the answer
 *
 * Exits non-zero if anything fails.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CI18N_THREAD_SHARED
#define CI18N_THREAD_SHARED
#endif

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define READERS 4
#define ITERATIONS 20000

/*
 * Tells the writer to stop once the readers are done.
 *
 * Behind a mutex rather than volatile. Volatile stops the compiler caching
 * the value, which is not the problem here: an unsynchronised write on one
 * thread and read on another is a data race whatever the qualifier says, and
 * ThreadSanitizer reports it. The flag is read 400 times in a whole run, so
 * the lock costs nothing worth measuring.
 */
static pthread_mutex_t stop_lock = PTHREAD_MUTEX_INITIALIZER;
static int stop_writing;

static void request_stop(void)
{
    pthread_mutex_lock(&stop_lock);
    stop_writing = 1;
    pthread_mutex_unlock(&stop_lock);
}

static int stop_requested(void)
{
    int stop;

    pthread_mutex_lock(&stop_lock);
    stop = stop_writing;
    pthread_mutex_unlock(&stop_lock);

    return stop;
}
static int failures;

static void fail(const char *what)
{
    printf("FAILED: %s\n", what);
    failures = 1;
}

/*
 * Readers only read. Every value in the catalogue is one of a known set, so a
 * reader that sees anything else has observed a state no writer ever
 * published.
 */
static void *reader(void *arg)
{
    long id = (long)(intptr_t)arg;
    int i;
    int seen = 0;

    for (i = 0; i < ITERATIONS; i++)
    {
        char buffer[64];
        size_t len = ci18n_get_copy("greeting", buffer, sizeof(buffer));

        if (len > 0)
        {
            if (strcmp(buffer, "first") != 0 && strcmp(buffer, "second") != 0)
            {
                printf("FAILED: reader saw [%s] (len %u), which no writer "
                       "published\n", buffer, (unsigned int)len);
                failures = 1;
                return NULL;
            }
            seen++;
        }

        /* A missing key sets this thread's error code. It must not be
         * visible to the writer, and the writer's must not show up here. */
        if (ci18n_get("definitely_absent") != NULL)
        {
            fail("an absent key was found");
            return NULL;
        }

        if (ci18n_last_error() != CI18N_ERR_KEY_NOT_FOUND)
        {
            fail("another thread overwrote this thread's error code");
            return NULL;
        }

        /* Exercise the rest of the read paths under contention. */
        ci18n_has("greeting");
        ci18n_count("en");
        ci18n_get_languages(NULL, 0);
        ci18n_plural_or_key("files", i);

        /* Formatting takes the same read lock and then runs application code
         * inside it, so it belongs under contention too. */
        {
            char formatted[64];
            size_t n = ci18n_format(formatted, sizeof(formatted), "shouted",
                                    "who", "world", NULL);

            if (n > 0 && strcmp(formatted, "hello, WORLD") != 0)
            {
                printf("FAILED: reader saw formatted [%s], which no writer "
                       "published\n", formatted);
                failures = 1;
                return NULL;
            }
        }
    }

    printf("reader %ld finished, saw %d values\n", id, seen);
    return NULL;
}

/*
 * A formatter runs inside ci18n_format while the read lock is held, on every
 * reader thread at once. So it must touch nothing shared and must not call
 * back into the library, which is exactly what the header asks of one.
 */
static size_t shout(char *out, size_t capacity, const char *value,
                    const char *arg, void *user_data)
{
    size_t len = strlen(value);
    size_t i;

    (void)arg;
    (void)user_data;

    for (i = 0; i < len && capacity > 0 && i < capacity - 1; i++)
    {
        char c = value[i];
        out[i] = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : c;
    }

    if (capacity > 0)
    {
        out[i] = '\0';
    }

    return len;
}

/* One writer, reloading and clearing while the readers run. */
static void *writer(void *arg)
{
    int i;

    (void)arg;

    for (i = 0; i < 400 && !stop_requested(); i++)
    {
        const char *first = "greeting=first\nfiles[one]=one file\nfiles[other]=many\n"
                            "shouted=hello, {who:shout}\n";
        const char *second = "greeting=second\nfiles[one]=1 file\nfiles[other]=lots\n"
                             "shouted=hello, {who:shout}\n";

        ci18n_load_from_buffer("en", first, strlen(first));
        ci18n_load_from_buffer("en", second, strlen(second));

        /* Clearing empties the language, so readers will see nothing for a
         * while. That is allowed; seeing rubbish is not. */
        if (i % 50 == 0)
        {
            ci18n_clear("en");
            ci18n_load_from_buffer("en", first, strlen(first));
            ci18n_set_current("en");
        }

        ci18n_set("en", "extra", "value");
        ci18n_remove("en", "extra");
    }

    return NULL;
}

int main(void)
{
    pthread_t readers[READERS];
    pthread_t scribe;
    const char *initial = "greeting=first\nfiles[one]=one file\nfiles[other]=many\n"
                          "shouted=hello, {who:shout}\n";
    long i;

    printf("=== ci18n shared context tests ===\n\n");

    if (!ci18n_init())
    {
        fail("init");
        return 1;
    }

    ci18n_load_from_buffer("en", initial, strlen(initial));
    ci18n_set_current("en");

    if (!ci18n_set_formatter("shout", shout, NULL))
    {
        fail("ci18n_set_formatter");
        return 1;
    }

    if (pthread_create(&scribe, NULL, writer, NULL) != 0)
    {
        fail("pthread_create writer");
        return 1;
    }

    for (i = 0; i < READERS; i++)
    {
        if (pthread_create(&readers[i], NULL, reader, (void *)(intptr_t)i) != 0)
        {
            fail("pthread_create reader");
            return 1;
        }
    }

    for (i = 0; i < READERS; i++)
    {
        pthread_join(readers[i], NULL);
    }

    request_stop();
    pthread_join(scribe, NULL);

    /* The catalogue has to be intact and usable afterwards. */
    ci18n_load_from_buffer("en", initial, strlen(initial));
    ci18n_set_current("en");

    if (!ci18n_get("greeting"))
    {
        fail("the catalogue did not survive the run");
    }

    ci18n_free();

    printf("\n%s\n", failures ? "=== FAILED ===" : "=== PASSED ===");
    return failures;
}
