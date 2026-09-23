/* SPDX-License-Identifier: MIT */
/*
 * How lookups scale across threads.
 *
 *   make bench-threads
 *
 * Built with CI18N_THREAD_SHARED, so every call takes its catalogue's read
 * lock. Readers never block each other, but readers of one catalogue all
 * update the same lock, and that is the cost this measures. The last column
 * gives each thread a catalogue of its own, which is the way around it.
 * Needs pthreads.
 */

#define _POSIX_C_SOURCE 200809L
#define CI18N_THREAD_SHARED
#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define KEYS 1000
#define OPS_PER_THREAD 2000000
#define MAX_THREADS 8

static char keys[KEYS][32];

/* shared is what every thread reads in the first two columns, own[i] is
 * thread i's private copy in the last one. */
static ci18n_t *shared;
static ci18n_t *own[MAX_THREADS];

typedef struct worker
{
    pthread_t thread;
    ci18n_t *catalog;
    int plural;      /* 0: ci18n_get_in, 1: ci18n_format_plural_in */
    double seconds;  /* written by the worker, read after join */
    size_t checksum; /* keeps the calls from being optimised away */
} worker_t;

static double now_seconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + (double)t.tv_nsec / 1e9;
}

static void *work(void *arg)
{
    worker_t *w = (worker_t *)arg;
    char out[64];
    size_t i, sum = 0;
    double start = now_seconds();

    for (i = 0; i < OPS_PER_THREAD; i++)
    {
        if (w->plural)
        {
            sum += ci18n_format_plural_in(w->catalog, out, sizeof(out), "files",
                                          (long)(i & 1023), NULL);
        }
        else
        {
            sum += (size_t)ci18n_get_in(w->catalog, keys[(i * 7919) % KEYS]);
        }
    }
    w->seconds = now_seconds() - start;
    w->checksum = sum;
    return NULL;
}

/* Runs `count` threads and returns the throughput in million calls a second.
 * Each thread times itself and the slowest one sets the wall time, so thread
 * start-up does not count against the result. */
static double run(int count, int plural, int private_catalogues)
{
    worker_t workers[MAX_THREADS];
    double slowest = 0.0;
    size_t checksum = 0;
    int i;

    for (i = 0; i < count; i++)
    {
        workers[i].catalog = private_catalogues ? own[i] : shared;
        workers[i].plural = plural;
        pthread_create(&workers[i].thread, NULL, work, &workers[i]);
    }
    for (i = 0; i < count; i++)
    {
        pthread_join(workers[i].thread, NULL);
        if (workers[i].seconds > slowest)
        {
            slowest = workers[i].seconds;
        }
        checksum += workers[i].checksum;
    }
    if (checksum == 0)
    {
        printf("nothing was looked up\n");
    }
    return (double)count * OPS_PER_THREAD / slowest / 1e6;
}

static ci18n_t *make_catalogue(void)
{
    ci18n_t *cat = ci18n_create();
    int i;
    for (i = 0; i < KEYS; i++)
    {
        ci18n_set_in(cat, "ru", keys[i], "Элемент {n} сохранён в папку");
    }
    ci18n_set_in(cat, "ru", "files[one]", "{count} файл");
    ci18n_set_in(cat, "ru", "files[few]", "{count} файла");
    ci18n_set_in(cat, "ru", "files[many]", "{count} файлов");
    ci18n_set_in(cat, "ru", "files[other]", "{count} файла");
    ci18n_set_current_in(cat, "ru");
    return cat;
}

int main(void)
{
    static const int counts[] = {1, 2, 4, 8};
    size_t c;
    int i;

    for (i = 0; i < KEYS; i++)
    {
        snprintf(keys[i], sizeof(keys[i]), "screen.item_%05d", i);
    }
    shared = make_catalogue();
    for (i = 0; i < MAX_THREADS; i++)
    {
        own[i] = make_catalogue();
    }

    printf("Million calls a second, all threads together.\n\n");
    printf("| Threads | `get`, one catalogue | `format_plural`, one catalogue | `get`, catalogue per thread |\n");
    printf("|---------|----------------------|--------------------------------|-----------------------------|\n");
    for (c = 0; c < sizeof(counts) / sizeof(counts[0]); c++)
    {
        double get = run(counts[c], 0, 0);
        double plural = run(counts[c], 1, 0);
        double own_get = run(counts[c], 0, 1);
        printf("| %7d | %20.1f | %30.1f | %27.1f |\n", counts[c], get, plural, own_get);
    }

    ci18n_destroy(shared);
    for (i = 0; i < MAX_THREADS; i++)
    {
        ci18n_destroy(own[i]);
    }
    return 0;
}
