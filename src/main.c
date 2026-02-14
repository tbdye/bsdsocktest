/*
 * bsdsocktest — Amiga bsdsocket.library conformance test suite
 *
 * Entry point: ReadArgs parsing, category dispatch, Ctrl-C handling.
 */

#include "tap.h"
#include "testutil.h"
#include "tests.h"

#include <proto/exec.h>
#include <proto/dos.h>

#include <stdio.h>
#include <string.h>

/* ReadArgs template */
#define TEMPLATE "CATEGORY/K,HOST/K,PORT/N,LOG/K,ALL/S,LOOPBACK/S,NETWORK/S,LIST/S,VERBOSE/S"

enum {
    ARG_CATEGORY,
    ARG_HOST,
    ARG_PORT,
    ARG_LOG,
    ARG_ALL,
    ARG_LOOPBACK,
    ARG_NETWORK,
    ARG_LIST,
    ARG_VERBOSE,
    ARG_COUNT
};

/* Test tier flags */
#define TIER_LOOPBACK 0x01
#define TIER_NETWORK  0x02
#define TIER_BOTH     (TIER_LOOPBACK | TIER_NETWORK)

/* Category table entry */
struct test_category {
    const char *name;
    void (*run)(void);
    int tier;
};

/* Category table — order matches test file structure */
static const struct test_category categories[] = {
    { "socket",     run_socket_tests,     TIER_LOOPBACK },
    { "sendrecv",   run_sendrecv_tests,   TIER_BOTH     },
    { "sockopt",    run_sockopt_tests,    TIER_LOOPBACK },
    { "waitselect", run_waitselect_tests, TIER_LOOPBACK },
    { "signals",    run_signals_tests,    TIER_LOOPBACK },
    { "dns",        run_dns_tests,        TIER_BOTH     },
    { "utility",    run_utility_tests,    TIER_LOOPBACK },
    { "transfer",   run_transfer_tests,   TIER_LOOPBACK },
    { "errno",      run_errno_tests,      TIER_LOOPBACK },
    { "misc",       run_misc_tests,       TIER_LOOPBACK },
    { "icmp",       run_icmp_tests,       TIER_NETWORK  },
    { "throughput", run_throughput_tests,  TIER_BOTH     },
    { NULL, NULL, 0 }
};

static void print_usage(void)
{
    printf("Usage: bsdsocktest [CATEGORY <name>] [ALL] [LOOPBACK] [NETWORK]\n"
           "                   [HOST <ip>] [PORT <num>] [LOG <path>] [VERBOSE]\n"
           "                   [LIST]\n\n"
           "  CATEGORY  Run a single test category by name\n"
           "  ALL       Run all test categories (default)\n"
           "  LOOPBACK  Run only loopback (self-contained) tests\n"
           "  NETWORK   Run only network tests (requires host helper)\n"
           "  HOST      Host helper IP address (default: not set)\n"
           "  PORT      Base port number (default: %d)\n"
           "  LOG       Duplicate TAP output to log file (AmigaOS path)\n"
           "  VERBOSE   Enable verbose diagnostics\n"
           "  LIST      List available test categories and exit\n",
           DEFAULT_BASE_PORT);
}

static void list_categories(void)
{
    const struct test_category *cat;

    printf("Available test categories:\n\n");
    printf("  %-12s  %s\n", "Name", "Tier");
    printf("  %-12s  %s\n", "----", "----");

    for (cat = categories; cat->name; cat++) {
        const char *tier;
        if (cat->tier == TIER_BOTH)
            tier = "loopback+network";
        else if (cat->tier == TIER_LOOPBACK)
            tier = "loopback";
        else
            tier = "network";
        printf("  %-12s  %s\n", cat->name, tier);
    }
}

/* Check if a category should be run based on the filter.
 * tier_filter: 0 = all, TIER_LOOPBACK = loopback only, etc.
 * cat_filter: NULL = all, otherwise must match name exactly. */
static int should_run(const struct test_category *cat,
                      int tier_filter, const char *cat_filter)
{
    if (cat_filter)
        return (stricmp(cat->name, cat_filter) == 0);

    if (tier_filter == 0)
        return 1;

    return (cat->tier & tier_filter) != 0;
}

int main(int argc, char **argv)
{
    struct RDArgs *rdargs;
    LONG args[ARG_COUNT];
    const struct test_category *cat;
    int tier_filter = 0;
    const char *cat_filter = NULL;
    int exit_code;
    int ran_any = 0;

    (void)argc;
    (void)argv;

    memset(args, 0, sizeof(args));

    rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rdargs) {
        print_usage();
        return RETURN_FAIL;
    }

    /* LIST mode — no library needed */
    if (args[ARG_LIST]) {
        list_categories();
        FreeArgs(rdargs);
        return RETURN_OK;
    }

    /* Parse options */
    if (args[ARG_PORT])
        set_base_port(*(LONG *)args[ARG_PORT]);

    if (args[ARG_CATEGORY])
        cat_filter = (const char *)args[ARG_CATEGORY];

    if (args[ARG_LOOPBACK])
        tier_filter = TIER_LOOPBACK;
    else if (args[ARG_NETWORK])
        tier_filter = TIER_NETWORK;
    /* ALL or no selection = run everything (tier_filter stays 0) */

    /* Set up verbose and log file before tap_init() so the TAP
     * header is captured in the log file too. */
    if (args[ARG_VERBOSE])
        tap_set_verbose(1);

    if (args[ARG_LOG])
        tap_set_logfile((const char *)args[ARG_LOG]);

    /* Open bsdsocket.library */
    if (open_bsdsocket() < 0) {
        tap_init(NULL);
        tap_plan(0);
        tap_bail("bsdsocket.library not available");
        exit_code = tap_finish();
        FreeArgs(rdargs);
        return exit_code;
    }

    /* Initialize TAP output */
    tap_init(get_bsdsocket_version());

    /* Dispatch categories */
    for (cat = categories; cat->name; cat++) {
        /* Check for Ctrl-C between categories */
        if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            tap_bail("Interrupted by Ctrl-C");
            break;
        }

        if (!should_run(cat, tier_filter, cat_filter))
            continue;

        tap_diagf("--- %s ---", cat->name);
        cat->run();
        ran_any = 1;
    }

    if (!ran_any && cat_filter) {
        tap_diagf("Unknown category: %s", cat_filter);
    }

    /* Emit trailing plan line (TAP v12 "plan at the end") */
    tap_plan(tap_get_total());

    exit_code = tap_finish();

    close_bsdsocket();
    FreeArgs(rdargs);

    return exit_code;
}
