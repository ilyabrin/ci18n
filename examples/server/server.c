/*
 * A threaded server, minus the network.
 *
 * Worker threads take requests off a queue, pick a language from each
 * request's Accept-Language header, and render a reply. Halfway through, a
 * reload thread swaps a translation in while readers are still reading.
 *
 * The shape to copy:
 *
 *   - CI18N_THREAD_SHARED, so any thread may read and one may write.
 *   - One catalogue per language, each with English as its fallback. A
 *     catalogue has one current language, and a server serves them all at
 *     once, so the language is chosen by picking a catalogue.
 *   - ci18n_get_copy_in and ci18n_format_*_in, never a bare pointer. A
 *     pointer from ci18n_get_in can dangle the moment a reload runs.
 *
 * SPDX-License-Identifier: MIT
 */

#define CI18N_THREAD_SHARED
#define CI18N_IMPLEMENTATION
#include "ci18n.h"

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------------
 * Translations
 *
 * Built in so the example runs from anywhere. A real server would call
 * ci18n_load_language_in on files instead; everything else stays the same.
 * ------------------------------------------------------------------------ */

static const char EN[] =
    "motd=Welcome back\n"
    "inbox[one]={name}, you have {count} new message\n"
    "inbox[other]={name}, you have {count} new messages\n"
    "rank[one]=You are {count}st in line\n"
    "rank[two]=You are {count}nd in line\n"
    "rank[few]=You are {count}rd in line\n"
    "rank[other]=You are {count}th in line\n"
    "balance=Balance: {amount:money} USD\n";

static const char RU[] =
    "motd=С возвращением\n"
    "inbox[one]={name}, у вас {count} новое сообщение\n"
    "inbox[few]={name}, у вас {count} новых сообщения\n"
    "inbox[many]={name}, у вас {count} новых сообщений\n"
    "rank[other]=Вы {count}-й в очереди\n"
    "balance=Баланс: {amount:money} USD\n";

/* No rank here on purpose: the reply falls back to English. */
static const char AR[] =
    "motd=مرحبا بعودتك\n"
    "inbox[zero]={name}، لا توجد رسائل جديدة\n"
    "inbox[one]={name}، لديك رسالة جديدة واحدة\n"
    "inbox[two]={name}، لديك رسالتان جديدتان\n"
    "inbox[few]={name}، لديك {count} رسائل جديدة\n"
    "inbox[many]={name}، لديك {count} رسالة جديدة\n"
    "inbox[other]={name}، لديك {count} رسالة جديدة\n"
    "balance=الرصيد: {amount:money} USD\n";

/* What the reload thread writes: a new message of the day for Russian. */
static const char RU_V2[] = "motd=Добро пожаловать снова\n";

/* ------------------------------------------------------------------------
 * A formatter: cents in, a decimal amount out
 *
 * The separator comes from user_data, registered once per catalogue, which
 * is how one function serves every language. It runs under the catalogue's
 * read lock, so it touches nothing but its arguments.
 * ------------------------------------------------------------------------ */

static size_t format_money(char *out, size_t capacity, const char *value,
                           const char *arg, void *user_data)
{
    const char *separator = (const char *)user_data;
    long cents = strtol(value, NULL, 10);
    int n;

    (void)arg;
    n = snprintf(out, capacity, "%ld%s%02ld", cents / 100, separator, cents % 100);
    return n < 0 ? 0 : (size_t)n;
}

/* ------------------------------------------------------------------------
 * Catalogues, one per language
 * ------------------------------------------------------------------------ */

typedef struct
{
    const char *code;
    const char *text;
    const char *decimal_separator;
    ci18n_t *cat;
} language_t;

static language_t LANGUAGES[] = {
    {"en", EN, ".", NULL},
    {"ru", RU, ",", NULL},
    {"ar", AR, ".", NULL},
};

#define LANGUAGE_COUNT (sizeof(LANGUAGES) / sizeof(LANGUAGES[0]))

static bool setup(void)
{
    size_t i;

    for (i = 0; i < LANGUAGE_COUNT; i++)
    {
        language_t *l = &LANGUAGES[i];

        l->cat = ci18n_create();
        if (!l->cat ||
            !ci18n_load_from_buffer_in(l->cat, "en", EN, strlen(EN)) ||
            !ci18n_load_from_buffer_in(l->cat, l->code, l->text, strlen(l->text)) ||
            !ci18n_set_current_in(l->cat, l->code) ||
            !ci18n_set_fallback_in(l->cat, "en") ||
            /* Replies are shown to people, some in Arabic, and user names
             * come in any script: isolate every filled-in value. */
            !ci18n_set_bidi_isolation_in(l->cat, true) ||
            !ci18n_set_formatter_in(l->cat, "money", format_money,
                                    (void *)l->decimal_separator))
        {
            return false;
        }
    }
    return true;
}

static void teardown(void)
{
    size_t i;

    for (i = 0; i < LANGUAGE_COUNT; i++)
    {
        ci18n_destroy(LANGUAGES[i].cat);
    }
}

/* ------------------------------------------------------------------------
 * Accept-Language
 *
 * "de;q=0.3, ru-RU;q=0.9, en;q=0.5": try tags by weight, highest first,
 * each as given and then as its primary subtag. Nothing matching means the
 * default language, which is what browsers expect.
 * ------------------------------------------------------------------------ */

static language_t *find_language(const char *tag, size_t len)
{
    size_t i;

    for (i = 0; i < LANGUAGE_COUNT; i++)
    {
        if (strlen(LANGUAGES[i].code) == len &&
            strncmp(LANGUAGES[i].code, tag, len) == 0)
        {
            return &LANGUAGES[i];
        }
    }
    return NULL;
}

static language_t *negotiate(const char *header)
{
    language_t *best = &LANGUAGES[0];
    double best_q = -1.0;
    const char *p = header;

    while (*p)
    {
        const char *tag;
        size_t tag_len;
        size_t primary_len;
        double q = 1.0;
        language_t *match;

        while (*p == ' ' || *p == ',')
        {
            p++;
        }
        tag = p;
        tag_len = strcspn(p, ";, ");
        p += tag_len;
        while (*p && *p != ',')
        {
            if (strncmp(p, ";q=", 3) == 0)
            {
                q = strtod(p + 3, NULL);
            }
            p++;
        }

        primary_len = strcspn(tag, "-_;, ");
        match = find_language(tag, tag_len);
        if (!match)
        {
            match = find_language(tag, primary_len);
        }
        /* Strictly greater, so equal weights keep the header's order. */
        if (match && q > best_q)
        {
            best = match;
            best_q = q;
        }
    }
    return best;
}

/* ------------------------------------------------------------------------
 * Requests
 * ------------------------------------------------------------------------ */

typedef struct
{
    const char *accept_language;
    const char *user;
    long unread;
    long position;
    const char *cents;
    char reply[512];
} request_t;

static request_t REQUESTS[] = {
    {"ru-RU,ru;q=0.9,en;q=0.8", "Анна", 3, 2, "150075", ""},
    {"en-US,en;q=0.5", "Bob", 1, 22, "999", ""},
    {"ar-EG", "ليلى", 11, 3, "4200", ""},
    {"fr-FR, de;q=0.7", "Chloé", 0, 13, "12", ""},
    {"de;q=0.3, ru;q=0.9", "Олег", 21, 1, "100", ""},
    {"ar;q=0.8, en-GB", "Sam", 2, 101, "50000", ""},
    {"ar", "Sam", 2, 1, "100", ""},
};

#define REQUEST_COUNT (sizeof(REQUESTS) / sizeof(REQUESTS[0]))

static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static size_t queue_next = 0;

static request_t *take_request(void)
{
    request_t *r = NULL;

    pthread_mutex_lock(&queue_lock);
    if (queue_next < REQUEST_COUNT)
    {
        r = &REQUESTS[queue_next++];
    }
    pthread_mutex_unlock(&queue_lock);
    return r;
}

static void handle(request_t *r)
{
    language_t *l = negotiate(r->accept_language);
    char motd[128];
    char inbox[160];
    char rank[96];
    char balance[96];

    /* Each call takes a read lock of its own, and copies into the caller's
     * buffer before letting go of it. */
    ci18n_get_copy_in(l->cat, "motd", motd, sizeof(motd));
    ci18n_format_plural_in(l->cat, inbox, sizeof(inbox), "inbox", r->unread,
                           "name", r->user, NULL);
    ci18n_format_ordinal_in(l->cat, rank, sizeof(rank), "rank", r->position, NULL);
    ci18n_format_in(l->cat, balance, sizeof(balance), "balance",
                    "amount", r->cents, NULL);

    /* The direction goes to the client as-is, for <html dir="...">. */
    (void)snprintf(r->reply, sizeof(r->reply), "lang=%s dir=%s\n  %s\n  %s\n  %s\n  %s",
                   l->code, ci18n_direction_name(ci18n_direction(l->code)),
                   motd, inbox, rank, balance);
}

static void *worker(void *arg)
{
    request_t *r;

    (void)arg;
    while ((r = take_request()) != NULL)
    {
        handle(r);
    }
    return NULL;
}

/* ------------------------------------------------------------------------
 * A reload while readers read
 * ------------------------------------------------------------------------ */

#define WORKERS 4
#define RELOADS 200

static int reload_done = 0; /* guarded by queue_lock */
static char motd_old[128];
static char motd_new[128];

static void *reloader(void *arg)
{
    language_t *ru = &LANGUAGES[1];
    int i;

    (void)arg;
    for (i = 0; i < RELOADS; i++)
    {
        /* A load merges into the language under one write lock, so readers
         * see the old text or the new one and nothing in between. Clearing
         * first and then loading would not be: a reader could land between
         * the two calls and find the key missing. */
        const char *text = (i % 2 == 1) ? RU_V2 : RU;

        ci18n_load_from_buffer_in(ru->cat, "ru", text, strlen(text));
    }
    pthread_mutex_lock(&queue_lock);
    reload_done = 1;
    pthread_mutex_unlock(&queue_lock);
    return NULL;
}

static void *reader(void *arg)
{
    language_t *ru = &LANGUAGES[1];
    long *torn = (long *)arg;
    char motd[128];
    int done = 0;

    while (!done)
    {
        ci18n_get_copy_in(ru->cat, "motd", motd, sizeof(motd));
        if (strcmp(motd, motd_old) != 0 && strcmp(motd, motd_new) != 0)
        {
            (*torn)++;
        }
        pthread_mutex_lock(&queue_lock);
        done = reload_done;
        pthread_mutex_unlock(&queue_lock);
    }
    return NULL;
}

int main(void)
{
    pthread_t threads[WORKERS];
    pthread_t writer;
    long torn[WORKERS] = {0};
    long torn_total = 0;
    size_t i;

    if (!setup())
    {
        fprintf(stderr, "setup failed\n");
        teardown();
        return 1;
    }

    /* 1. Serve every request on a pool of workers. Replies are printed
     *    afterwards, in request order, so the output does not depend on
     *    which worker ran first. */
    for (i = 0; i < WORKERS; i++)
    {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
    for (i = 0; i < WORKERS; i++)
    {
        pthread_join(threads[i], NULL);
    }
    for (i = 0; i < REQUEST_COUNT; i++)
    {
        printf("[%u] %s\n  %s\n", (unsigned)(i + 1), REQUESTS[i].accept_language,
               REQUESTS[i].reply);
    }

    /* 2. Swap the Russian message of the day back and forth while readers
     *    keep reading it. Each reader checks that every copy is one version
     *    or the other, never a mix. */
    ci18n_get_copy_in(LANGUAGES[1].cat, "motd", motd_old, sizeof(motd_old));
    (void)snprintf(motd_new, sizeof(motd_new), "%s", RU_V2 + strlen("motd="));
    motd_new[strcspn(motd_new, "\n")] = '\0';

    for (i = 0; i < WORKERS; i++)
    {
        pthread_create(&threads[i], NULL, reader, &torn[i]);
    }
    pthread_create(&writer, NULL, reloader, NULL);
    pthread_join(writer, NULL);
    for (i = 0; i < WORKERS; i++)
    {
        pthread_join(threads[i], NULL);
        torn_total += torn[i];
    }

    ci18n_get_copy_in(LANGUAGES[1].cat, "motd", motd_old, sizeof(motd_old));
    printf("\nreload: %d reloads under %d readers, %ld torn reads\n",
           RELOADS, WORKERS, torn_total);
    printf("ru motd is now: %s\n", motd_old);

    teardown();
    return torn_total == 0 ? 0 : 1;
}
