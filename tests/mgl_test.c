/*
 * mgl_test.c
 * MGL
 *
 * Test runner. Usage:
 *   mgl_tests [--list] [--filter PAT] [--no-gpu] [--verbose] [--json FILE]
 */

#include <stdarg.h>
#include <time.h>

#include "mgl_test.h"
#include "harness.h"

int mgl_test_failures = 0;
int mgl_test_checks   = 0;
int mgl_test_skipped  = 0;
const char *mgl_test_current = NULL;

static MGLTestCase *g_head = NULL;
static MGLTestCase *g_tail = NULL;
static int g_verbose = 0;
static int g_this_skipped = 0;
static char g_skip_reason[256];

void mgl_test_register(MGLTestCase *tc)
{
    tc->next = NULL;

    if (g_tail) g_tail->next = tc;
    else        g_head = tc;

    g_tail = tc;
}

void mgl_test_fail(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    const char *base = strrchr(file, '/');

    mgl_test_failures++;

    fprintf(stderr, "    FAIL %s:%d: ", base ? base + 1 : file, line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}

void mgl_test_skip(const char *reason)
{
    g_this_skipped = 1;
    snprintf(g_skip_reason, sizeof g_skip_reason, "%s", reason ? reason : "");
}

int mgl_test_is_skipped(void) { return g_this_skipped; }

// pat is one substring, or several separated by commas (matches any of them)
static int matches(const char *pat, const MGLTestCase *tc)
{
    char full[256];
    const char *p = pat;

    if (!pat || !*pat) return 1;

    snprintf(full, sizeof full, "%s.%s", tc->suite, tc->name);

    while (*p)
    {
        const char *comma = strchr(p, ',');
        size_t len = comma ? (size_t)(comma - p) : strlen(p);
        char one[128];

        if (len && len < sizeof one)
        {
            memcpy(one, p, len);
            one[len] = 0;

            if (strstr(full, one)) return 1;
        }

        if (!comma) break;
        p = comma + 1;
    }

    return 0;
}

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
}

int main(int argc, char **argv)
{
    const char *filter = NULL, *json_path = NULL;
    int list_only = 0, allow_gpu = 1;
    unsigned shuffle_seed = 0;
    int do_shuffle = 0;
    int passed = 0, failed = 0, skipped = 0, total = 0;
    FILE *json = NULL;
    double t0;

    setvbuf(stdout, NULL, _IONBF, 0);

    for (int i = 1; i < argc; i++)
    {
        if      (!strcmp(argv[i], "--list"))    list_only = 1;
        else if (!strcmp(argv[i], "--no-gpu"))  allow_gpu = 0;
        else if (!strcmp(argv[i], "--verbose")) g_verbose = 1;
        else if (!strcmp(argv[i], "--filter") && i + 1 < argc) filter = argv[++i];
        else if (!strcmp(argv[i], "--json")   && i + 1 < argc) json_path = argv[++i];
        else if (!strcmp(argv[i], "--shuffle") && i + 1 < argc)
        {
            do_shuffle = 1;
            shuffle_seed = (unsigned)strtoul(argv[++i], NULL, 10);
        }
        else if (!strcmp(argv[i], "--help"))
        {
            printf("usage: %s [--list] [--filter PAT[,PAT...]] [--no-gpu] [--verbose]\n"
                   "          [--shuffle SEED] [--json FILE]\n", argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            return 2;
        }
    }

    for (MGLTestCase *tc = g_head; tc; tc = tc->next)
        if (matches(filter, tc)) total++;

    if (list_only)
    {
        for (MGLTestCase *tc = g_head; tc; tc = tc->next)
            if (matches(filter, tc))
                printf("%s.%s%s\n", tc->suite, tc->name, tc->needs_gpu ? " [gpu]" : "");

        return 0;
    }

    if (json_path)
    {
        json = fopen(json_path, "w");
        if (json) fprintf(json, "{\n  \"tests\": [\n");
    }

    // Materialise the run order so it can be permuted. Registration order is
    // just link order, so a suite that only passes in that one order is hiding
    // a dependency between tests.
    MGLTestCase **run = (MGLTestCase **)calloc((size_t)(total ? total : 1), sizeof *run);
    int run_count = 0;

    if (!run) return 2;

    for (MGLTestCase *tc = g_head; tc; tc = tc->next)
        if (matches(filter, tc)) run[run_count++] = tc;

    if (do_shuffle)
    {
        unsigned st = shuffle_seed ? shuffle_seed : 1u;

        for (int i = run_count - 1; i > 0; i--)
        {
            st = st * 1664525u + 1013904223u;         // numerical recipes LCG

            int j = (int)((st >> 16) % (unsigned)(i + 1));
            MGLTestCase *t = run[i]; run[i] = run[j]; run[j] = t;
        }

        printf("shuffled with seed %u\n", shuffle_seed);
    }

    printf("running %d test%s\n\n", total, total == 1 ? "" : "s");

    // one context for every GPU test; creating one per test is slow and leaks
    int gpu_ready = 0;
    if (allow_gpu)
        gpu_ready = mgl_harness_init();

    t0 = now_ms();

    int first_json = 1;
    for (int ti = 0; ti < run_count; ti++)
    {
        MGLTestCase *tc = run[ti];
        int before = mgl_test_failures;
        double tt0 = now_ms();
        const char *status;

        g_this_skipped = 0;
        g_skip_reason[0] = 0;
        mgl_test_current = tc->name;

        if (tc->needs_gpu && !gpu_ready)
        {
            g_this_skipped = 1;
            snprintf(g_skip_reason, sizeof g_skip_reason, "no GPU context");
        }
        else
        {
            if (tc->needs_gpu) mgl_harness_reset();
            tc->fn();
        }

        double dt = now_ms() - tt0;

        if (g_this_skipped)      { skipped++; status = "skip"; }
        else if (mgl_test_failures > before) { failed++;  status = "fail"; }
        else                     { passed++;  status = "pass"; }

        if (g_verbose || strcmp(status, "pass") != 0)
            printf("  %-4s %s.%s%s%s (%.1f ms)\n", status, tc->suite, tc->name,
                   g_skip_reason[0] ? " - " : "", g_skip_reason, dt);

        if (json)
        {
            fprintf(json, "%s    {\"suite\": \"%s\", \"name\": \"%s\", \"status\": \"%s\", \"ms\": %.3f}",
                    first_json ? "" : ",\n", tc->suite, tc->name, status, dt);
            first_json = 0;
            fflush(json);   // a crashing test must not take the results with it
        }
    }

    double total_ms = now_ms() - t0;

    free(run);

    if (allow_gpu && gpu_ready)
        mgl_harness_shutdown();

    printf("\n%d passed, %d failed, %d skipped  (%d checks, %.0f ms)\n",
           passed, failed, skipped, mgl_test_checks, total_ms);

    if (json)
    {
        fprintf(json, "\n  ],\n  \"passed\": %d,\n  \"failed\": %d,\n  \"skipped\": %d,\n  \"checks\": %d\n}\n",
                passed, failed, skipped, mgl_test_checks);
        fclose(json);
    }

    return failed ? 1 : 0;
}
