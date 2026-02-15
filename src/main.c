/*
 * bsdsocktest — Amiga bsdsocket.library conformance test suite
 *
 * Entry point: ReadArgs parsing, category dispatch, Ctrl-C handling.
 */

#include "tap.h"
#include "testutil.h"
#include "tests.h"
#include "helper_proto.h"
#include "known_failures.h"

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <workbench/startup.h>

#include <stdio.h>
#include <string.h>

/* Ensure sufficient stack for test buffers and nested calls.
 * libnix startup code checks this and expands the stack if needed. */
unsigned long __stack = 65536;

/* Override libnix default CON: window for Workbench launches.
 * libnix opens this window before main() when argc==0.
 * Overrides the default "CON://///AUTO/CLOSE/WAIT" from libnix.
 * This is compile-time; WBTOOL icons cannot change it via Tool Types. */
const char *__stdiowin = "CON:0/20/640/180/bsdsocktest/AUTO/CLOSE/WAIT";

/* icon.library base — needed by proto/icon.h inline stubs */
struct Library *IconBase = NULL;

/* ReadArgs template */
#define TEMPLATE "CATEGORY/K,HOST/K,PORT/N,LOG/K,ALL/S,LOOPBACK/S,NETWORK/S,LIST/S,VERBOSE/S,NOPAGE/S"

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
    ARG_NOPAGE,
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
    const char *description;
};

/* Category table — order matches test file structure */
static const struct test_category categories[] = {
    { "socket",     run_socket_tests,     TIER_LOOPBACK,
      "Core socket lifecycle: create, bind, listen, connect, accept, close" },
    { "sendrecv",   run_sendrecv_tests,   TIER_BOTH,
      "Data transfer: send, recv, sendto, recvfrom, sendmsg, recvmsg" },
    { "sockopt",    run_sockopt_tests,    TIER_LOOPBACK,
      "Socket options: getsockopt, setsockopt, IoctlSocket" },
    { "waitselect", run_waitselect_tests, TIER_LOOPBACK,
      "Async I/O: WaitSelect readiness, timeout, signal integration" },
    { "signals",    run_signals_tests,    TIER_LOOPBACK,
      "Signals and events: SetSocketSignals, SocketBaseTags, GetSocketEvents" },
    { "dns",        run_dns_tests,        TIER_BOTH,
      "Name resolution: gethostbyname/addr, getservby*, getprotoby*" },
    { "utility",    run_utility_tests,    TIER_LOOPBACK,
      "Address utilities: Inet_NtoA, inet_addr, Inet_LnaOf, Inet_NetOf" },
    { "transfer",   run_transfer_tests,   TIER_LOOPBACK,
      "Descriptor transfer: Dup2Socket, ObtainSocket, ReleaseSocket" },
    { "errno",      run_errno_tests,      TIER_LOOPBACK,
      "Error handling: Errno, SetErrnoPtr, SocketBaseTags errno pointers" },
    { "misc",       run_misc_tests,       TIER_LOOPBACK,
      "Miscellaneous: getdtablesize, syslog, resource limits" },
    { "icmp",       run_icmp_tests,       TIER_BOTH,
      "ICMP echo: raw socket ping, RTT measurement" },
    { "throughput", run_throughput_tests,  TIER_BOTH,
      "Throughput benchmarks: TCP/UDP loopback and network transfer" },
    { NULL, NULL, 0, NULL }
};

static void print_usage(void)
{
    printf("Usage: bsdsocktest [CATEGORY <name>] [ALL] [LOOPBACK] [NETWORK]\n"
           "                   [HOST <ip>] [PORT <num>] [LOG <path>] [VERBOSE]\n"
           "                   [NOPAGE] [LIST]\n\n"
           "  CATEGORY  Run a single test category by name\n"
           "  ALL       Run all test categories (default)\n"
           "  LOOPBACK  Run only loopback (self-contained) tests\n"
           "  NETWORK   Run only network tests (requires host helper)\n"
           "  HOST      Host helper IP address (default: not set)\n"
           "  PORT      Base port number (default: %d)\n"
           "  LOG       Log file path (default: bsdsocktest.log, NIL: to suppress)\n"
           "  VERBOSE   Show individual test results on screen\n"
           "  NOPAGE    Disable pagination (output scrolls freely)\n"
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
    const char *log_path;
    int exit_code;
    int ran_any = 0;

    /* Workbench startup variables (C89: declare before any code) */
    struct RDArgs wb_rda;
    char argbuf[512];

    memset(args, 0, sizeof(args));

    if (argc == 0) {
        /* Workbench launch: build CLI-style arg string from Tool Types.
         * libnix has already: waited for WBStartup, opened our CON:
         * window via __stdiowin, and called CurrentDir() to the
         * program's directory. argv is the WBStartup message. */
        struct WBStartup *wbmsg = (struct WBStartup *)argv;
        struct DiskObject *dobj = NULL;
        CONST_STRPTR *tt;
        char *p = argbuf;
        UBYTE *val;

        IconBase = OpenLibrary((STRPTR)"icon.library", 36);
        if (IconBase) {
            dobj = GetDiskObject(wbmsg->sm_ArgList[0].wa_Name);
            if (dobj && dobj->do_ToolTypes) {
                tt = (CONST_STRPTR *)dobj->do_ToolTypes;
                val = FindToolType(tt, (STRPTR)"HOST");
                if (val)
                    p += sprintf(p, "HOST %s ", (char *)val);
                if (FindToolType(tt, (STRPTR)"LOOPBACK"))
                    p += sprintf(p, "LOOPBACK ");
                if (FindToolType(tt, (STRPTR)"ALL"))
                    p += sprintf(p, "ALL ");
                if (FindToolType(tt, (STRPTR)"NETWORK"))
                    p += sprintf(p, "NETWORK ");
                if (FindToolType(tt, (STRPTR)"VERBOSE"))
                    p += sprintf(p, "VERBOSE ");
                if (FindToolType(tt, (STRPTR)"NOPAGE"))
                    p += sprintf(p, "NOPAGE ");
                val = FindToolType(tt, (STRPTR)"LOG");
                if (val)
                    p += sprintf(p, "LOG %s ", (char *)val);
                val = FindToolType(tt, (STRPTR)"PORT");
                if (val)
                    p += sprintf(p, "PORT %s ", (char *)val);
                val = FindToolType(tt, (STRPTR)"CATEGORY");
                if (val)
                    p += sprintf(p, "CATEGORY %s ", (char *)val);
            }
            if (dobj)
                FreeDiskObject(dobj);
            CloseLibrary(IconBase);
            IconBase = NULL;
        }
        *p++ = '\n';
        *p = '\0';

        /* Feed argbuf to ReadArgs via RDA_Source */
        memset(&wb_rda, 0, sizeof(wb_rda));
        wb_rda.RDA_Source.CS_Buffer = (STRPTR)argbuf;
        wb_rda.RDA_Source.CS_Length = strlen(argbuf);
        rdargs = ReadArgs((STRPTR)TEMPLATE, args, &wb_rda);
    } else {
        /* CLI launch: standard ReadArgs from command line */
        rdargs = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    }

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

    if (args[ARG_VERBOSE])
        tap_set_verbose(1);

    if (!args[ARG_NOPAGE])
        tap_set_page(1);

    /* Determine log file path (NULL = default "bsdsocktest.log") */
    log_path = args[ARG_LOG] ? (const char *)args[ARG_LOG] : NULL;

    /* Open bsdsocket.library */
    if (open_bsdsocket() < 0) {
        tap_init(NULL, log_path);
        tap_plan(0);
        tap_bail("bsdsocket.library not available");
        exit_code = tap_finish();
        FreeArgs(rdargs);
        return exit_code;
    }

    /* Clean up any leaked sockets from previous runs */
    reset_socket_state();

    /* Initialize TAP output */
    tap_init(get_bsdsocket_version(), log_path);

    /* Initialize high-resolution timing */
    if (timer_init() < 0) {
        tap_plan(0);
        tap_bail("timer.device not available");
        exit_code = tap_finish();
        close_bsdsocket();
        FreeArgs(rdargs);
        return exit_code;
    }

    /* Initialize known-failures table for the detected stack */
    known_init(get_bsdsocket_version());

    /* Connect to host helper if HOST was specified.
     * Bail out on failure — the user explicitly requested network tests. */
    if (args[ARG_HOST]) {
        const char *host_str = (const char *)args[ARG_HOST];
        if (!helper_connect(host_str)) {
            tap_diagf("host=%s, port=%d", host_str, HELPER_CTRL_PORT);
            tap_plan(0);
            tap_bail("Could not connect to host helper");
            exit_code = tap_finish();
            timer_cleanup();
            close_bsdsocket();
            FreeArgs(rdargs);
            return exit_code;
        }
    }

    /* Dispatch categories */
    for (cat = categories; cat->name; cat++) {
        /* Check for Ctrl-C between categories */
        if (SetSignal(0L, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) {
            tap_bail("Interrupted by Ctrl-C");
            break;
        }

        if (!should_run(cat, tier_filter, cat_filter))
            continue;

        tap_begin_category(cat->name);
        if (cat->description)
            tap_diag(cat->description);
        ran_any = 1;
        cat->run();

        if (tap_bailed())
            break;

        tap_end_category();
    }

    if (!ran_any && cat_filter) {
        tap_diagf("Unknown category: %s", cat_filter);
    }

    /* Disconnect from host helper */
    helper_quit();

    /* Emit trailing plan line (TAP v12 "plan at the end") */
    tap_plan(tap_get_total());

    exit_code = tap_finish();

    timer_cleanup();
    close_bsdsocket();
    FreeArgs(rdargs);

    return exit_code;
}
