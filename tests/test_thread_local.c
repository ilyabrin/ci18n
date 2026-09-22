/*
 * Thread-local context tests.
 *
 *   make test-threads
 *
 * Separate from the main suite because it needs pthreads, and because it only
 * makes sense with CI18N_THREAD_LOCAL_CONTEXT defined.
 *
 * This exists because the macro used to be selected by OS: MinGW gcc defines
 * _WIN32 but ignores __declspec(thread), so every thread silently shared one
 * global context. Nothing in CI compiled this path, so nothing caught it.
 *
 * Exits non-zero if any check fails.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef CI18N_THREAD_LOCAL_CONTEXT
#define CI18N_THREAD_LOCAL_CONTEXT
#endif

#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#define WORKERS 4
#define ITERATIONS 2000

static int failures;

#define CHECK(cond)                                          \
    do                                                       \
    {                                                        \
        if (!(cond))                                         \
        {                                                    \
            printf("FAILED: %s\n  at %s:%d\n",               \
                   #cond, __FILE__, __LINE__);               \
            failures = 1;                                    \
        }                                                    \
    } while (0)

struct worker_arg
{
    char lang[8];
    char value[32];
    int ok;
};

/*
 * Each worker owns its context: it must start empty, and the value it writes
 * must never be visible to any other thread. With a shared context the
 * WORKERS threads would clobber the same "k" entry and these reads would come
 * back with another thread's value.
 */
static void *worker(void *raw)
{
    struct worker_arg *arg = (struct worker_arg *)raw;
    int i;

    arg->ok = 1;

    /* A fresh thread has never been initialized, whatever main did. */
    if (ci18n_is_initialized())
    {
        arg->ok = 0;
        return NULL;
    }

    ci18n_init();
    ci18n_set(arg->lang, "k", arg->value);
    ci18n_set_current(arg->lang);

    for (i = 0; i < ITERATIONS; i++)
    {
        const char *got = ci18n_get("k");

        if (!got || strcmp(got, arg->value) != 0)
        {
            arg->ok = 0;
            break;
        }

        /* Only this thread's language should exist here. */
        if (ci18n_get_languages(NULL, 0) != 1)
        {
            arg->ok = 0;
            break;
        }
    }

    ci18n_free();
    return NULL;
}

int main(void)
{
    pthread_t threads[WORKERS];
    struct worker_arg args[WORKERS];
    int i;

    printf("=== ci18n thread-local context tests ===\n\n");

    ci18n_init();
    ci18n_set("en", "k", "main-value");
    ci18n_set_current("en");

    for (i = 0; i < WORKERS; i++)
    {
        snprintf(args[i].lang, sizeof(args[i].lang), "l%d", i);
        snprintf(args[i].value, sizeof(args[i].value), "worker-%d", i);
        args[i].ok = 0;

        CHECK(pthread_create(&threads[i], NULL, worker, &args[i]) == 0);
    }

    for (i = 0; i < WORKERS; i++)
    {
        pthread_join(threads[i], NULL);
        printf("worker %d isolated: %s\n", i, args[i].ok ? "yes" : "no");
        CHECK(args[i].ok);
    }

    /* Nothing the workers loaded may have reached this context. */
    CHECK(ci18n_get_languages(NULL, 0) == 1);
    CHECK(ci18n_get("k") != NULL);
    CHECK(strcmp(ci18n_get_or_key("k"), "main-value") == 0);

    ci18n_free();

    printf("\n%s\n", failures ? "=== FAILED ===" : "=== PASSED ===");
    return failures;
}
