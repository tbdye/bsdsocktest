/*
 * bsdsocktest — TAP (Test Anything Protocol) v12 output framework
 *
 * Dual-output architecture: compact dashboard on screen (stdout),
 * full TAP v12 detail in a log file.
 */

#include "tap.h"
#include "known_failures.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <dos/dosextens.h>
#include <devices/conunit.h>

/* Category name column width for dot-padding on screen */
#define CAT_WIDTH 23

/* Maximum unexpected failures to expand on screen per category */
#define MAX_FAILURES_DISPLAY 16

/* Maximum notable results per category */
#define MAX_NOTES 8

/* CSI bold on / bold off (AmigaOS native, works in all CON: windows) */
#define CSI_BOLD  "\x9B" "1m"
#define CSI_RESET "\x9B" "0m"

/* ---- Global state ---- */

static int test_number;        /* Global test counter (all tests) */
static int passed_count;       /* Clean passes + skips */
static int failed_count;       /* Unexpected failures */
static int known_count;        /* Known stack limitations */
static int skipped_count;      /* Skipped tests */
static int bailed_out;
static int verbose;
static FILE *logfp;

/* Per-category tracking (reset by tap_begin_category) */
static char current_category[32];
static int cat_passed;
static int cat_failed;
static int cat_known;
static int cat_skipped;
static int cat_total;

/* Failed test descriptions for screen expansion (unexpected only) */
static struct {
    int test_num;
    char description[128];
} cat_failures[MAX_FAILURES_DISPLAY];
static int cat_failure_count;

/* Notable results for screen display under category */
static char cat_notes[MAX_NOTES][128];
static int cat_note_count;

/* Pagination state */
static int page_mode;       /* pagination enabled */
static int screen_height;   /* detected rows (see detect_screen_size) */
static int screen_width;    /* detected columns, 0 if unknown */
static int lines_printed;   /* screen rows since last page break */

/* Global screen counters (accumulated from tap_end_category) */
static int screen_passed;
static int screen_failed;
static int screen_known;
static int screen_skipped;

/* ---- Internal helpers ---- */

/* Write formatted output to log file only */
static void log_printf(const char *fmt, ...)
{
    va_list ap;

    if (!logfp)
        return;

    va_start(ap, fmt);
    vfprintf(logfp, fmt, ap);
    va_end(ap);
}

/* Write a line to log file only (adds newline) */
static void log_puts(const char *line)
{
    if (!logfp)
        return;

    fputs(line, logfp);
    fputc('\n', logfp);
}

/* Print category name with dot-padding to screen */
static void print_cat_dots(const char *name)
{
    int name_len, dots, i;

    name_len = strlen(name);
    printf("%s", name);
    dots = CAT_WIDTH - name_len;
    if (dots < 3)
        dots = 3;
    for (i = 0; i < dots; i++)
        putchar('.');
}

/* Print parenthetical detail suffix.
 * Only prints if at least one count is non-zero. */
static void print_detail_suffix(int unexpected, int known, int skipped)
{
    int need_comma = 0;

    if (unexpected == 0 && known == 0 && skipped == 0)
        return;

    printf(" (");
    if (unexpected > 0) {
        printf("%d failed", unexpected);
        need_comma = 1;
    }
    if (known > 0) {
        if (need_comma) printf(", ");
        printf("%d known %s", known, known == 1 ? "issue" : "issues");
        need_comma = 1;
    }
    if (skipped > 0) {
        if (need_comma) printf(", ");
        printf("%d skipped", skipped);
    }
    printf(")");
}

/* Detect screen dimensions via ACTION_DISK_INFO + ConUnit.
 * Reads the console window's actual character dimensions
 * without moving the cursor or causing any screen output.
 * Sets screen_width as a side effect.
 * Returns usable row count for pagination, or 0 on failure. */
static int detect_screen_size(void)
{
    struct FileHandle *cfh;
    struct InfoData *id;
    struct IOStdReq *con_io;
    struct ConUnit *con_unit;
    int rows = 0;

    screen_width = 0;

    cfh = (struct FileHandle *)BADDR(Output());
    if (!cfh || !cfh->fh_Type)
        return 0;

    /* InfoData must be longword-aligned; AllocMem guarantees this */
    id = (struct InfoData *)AllocMem(sizeof(struct InfoData),
                                     MEMF_PUBLIC | MEMF_CLEAR);
    if (!id)
        return 0;

    if (DoPkt(cfh->fh_Type, ACTION_DISK_INFO, MKBADDR(id), 0, 0, 0, 0)) {
        con_io = (struct IOStdReq *)id->id_InUse;
        if (con_io) {
            con_unit = (struct ConUnit *)con_io->io_Unit;
            if (con_unit) {
                /* cu_YMax is 0-based, so cu_YMax alone is one less
                 * than the true row count.  We use it as-is to
                 * reserve a line for the pagination prompt, keeping
                 * all content visible without scrolling off. */
                rows = (int)con_unit->cu_YMax;
                screen_width = (int)con_unit->cu_XMax + 1;
            }
        }
    }

    FreeMem(id, sizeof(struct InfoData));
    return rows;
}

/* How many screen rows does a line of visible_chars characters occupy? */
static int wrap_rows(int visible_chars)
{
    if (screen_width <= 0 || visible_chars <= screen_width)
        return 1;
    return (visible_chars + screen_width - 1) / screen_width;
}

/* Advance the page line counter by 'rows' screen rows.
 * Triggers the pagination prompt when the screen is full. */
static void page_advance(int rows)
{
    BPTR in;
    char ch;

    if (!page_mode || screen_height <= 0)
        return;

    lines_printed += rows;
    if (lines_printed < screen_height - 1)
        return;

    in = Input();

    printf("-- Enter for more, Q for all, Ctrl-C to stop --");
    fflush(stdout);

    /* Raw mode: single keypress without waiting for Enter */
    SetMode(in, 1);

    if (Read(in, &ch, 1) == 1) {
        if (ch == 'q' || ch == 'Q') {
            page_mode = 0;
        } else if (ch == 0x03) {
            /* Ctrl-C: signal break for main loop to catch */
            SetSignal(SIGBREAKF_CTRL_C, SIGBREAKF_CTRL_C);
            page_mode = 0;
        }
        /* Enter (\r) or any other key: continue paging */
    } else {
        /* EOF or error on stdin — disable pagination */
        page_mode = 0;
    }

    SetMode(in, 0);

    /* Clear prompt line — cursor stays on the same row */
    printf("\r%*s\r", 50, "");
    lines_printed = 0;
}

/* Check pagination after printing a single short line to screen */
static void page_check(void)
{
    page_advance(1);
}

/* ---- Public API ---- */

void tap_init(const char *bsdlib_version, const char *log_path)
{
    int is_nil;

    /* Reset all state */
    test_number = 0;
    passed_count = 0;
    failed_count = 0;
    known_count = 0;
    skipped_count = 0;
    bailed_out = 0;
    current_category[0] = '\0';
    screen_passed = 0;
    screen_failed = 0;
    screen_known = 0;
    screen_skipped = 0;

    /* Open log file */
    if (!log_path)
        log_path = "bsdsocktest.log";

    is_nil = (stricmp(log_path, "NIL:") == 0);

    logfp = fopen(log_path, "w");
    if (!logfp && !is_nil)
        printf("Warning: could not open log file %s\n", log_path);

    /* Log: full TAP header */
    log_puts("TAP version 12");
    log_printf("# bsdsocktest %s\n", BSDSOCKTEST_VERSION);
    if (bsdlib_version)
        log_printf("# bsdsocket.library: %s\n", bsdlib_version);
    else
        log_puts("# bsdsocket.library: not available");

    /* Log: pagination diagnostics for debugging screen issues */
    if (page_mode)
        log_printf("# page: height=%d width=%d\n", screen_height, screen_width);

    /* Screen: compact header (bold) */
    if (bsdlib_version)
        printf(CSI_BOLD "bsdsocktest %s - %s" CSI_RESET "\n",
               BSDSOCKTEST_VERSION, bsdlib_version);
    else
        printf(CSI_BOLD "bsdsocktest %s" CSI_RESET "\n",
               BSDSOCKTEST_VERSION);
    page_check();

    /* Show log file path (suppress for NIL:) */
    if (!is_nil && logfp) {
        printf("Log: %s\n", log_path);
        page_check();
    }

    printf("\n");
    page_check();
}

void tap_plan(int count)
{
    log_printf("1..%d\n", count);
}

void tap_ok(int passed, const char *description)
{
    const char *kr;
    int in_cat;

    test_number++;
    in_cat = (current_category[0] != '\0');

    if (in_cat)
        cat_total++;

    kr = known_check(test_number);

    if (passed) {
        if (kr) {
            /* Known limitation unexpectedly passed — stack may have
             * been updated.  Log it with annotation for visibility. */
            log_printf("ok %d - %s  # KNOWN %s: %s\n",
                       test_number, description, known_stack_name(), kr);
        } else {
            /* Normal pass */
            log_printf("ok %d - %s\n", test_number, description);
        }
        passed_count++;
        if (in_cat)
            cat_passed++;
    } else {
        if (kr) {
            /* Known stack limitation — expected failure */
            log_printf("not ok %d - %s  # KNOWN %s: %s\n",
                       test_number, description, known_stack_name(), kr);
            known_count++;
            if (in_cat)
                cat_known++;
        } else {
            /* Unexpected failure */
            log_printf("not ok %d - %s\n",
                       test_number, description);
            failed_count++;
            if (in_cat) {
                cat_failed++;
                if (cat_failure_count < MAX_FAILURES_DISPLAY) {
                    cat_failures[cat_failure_count].test_num = test_number;
                    strncpy(cat_failures[cat_failure_count].description,
                            description, 127);
                    cat_failures[cat_failure_count].description[127] = '\0';
                    cat_failure_count++;
                }
            }
        }
    }

    /* Verbose: show individual test line on screen (number-first) */
    if (verbose) {
        int line_len;

        if (passed) {
            line_len = printf("  %3d ok    - %s\n", test_number, description);
        } else if (kr) {
            line_len = printf("  %3d KNOWN - %s\n", test_number, description);
        } else {
            line_len = printf("  %3d FAIL  - %s\n", test_number, description);
        }
        /* Account for line wrapping: line_len - 1 excludes the newline */
        page_advance(wrap_rows(line_len > 1 ? line_len - 1 : 1));
    }
}

void tap_okf(int passed, const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    tap_ok(passed, buf);
}

void tap_skip(const char *reason)
{
    int in_cat;

    test_number++;
    passed_count++;
    skipped_count++;
    in_cat = (current_category[0] != '\0');

    if (in_cat) {
        cat_total++;
        cat_skipped++;
    }

    log_printf("ok %d - # SKIP %s\n", test_number, reason);

    if (verbose) {
        int line_len;

        line_len = printf("  %3d skip - %s\n", test_number, reason);
        page_advance(wrap_rows(line_len > 1 ? line_len - 1 : 1));
    }
}

void tap_diag(const char *msg)
{
    log_printf("# %s\n", msg);
}

void tap_diagf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    log_printf("# %s\n", buf);
}

void tap_note(const char *msg)
{
    /* Log: as diagnostic */
    log_printf("# %s\n", msg);

    /* Screen: store for display under category summary */
    if (current_category[0] != '\0' && cat_note_count < MAX_NOTES) {
        strncpy(cat_notes[cat_note_count], msg, 127);
        cat_notes[cat_note_count][127] = '\0';
        cat_note_count++;
    }
}

void tap_notef(const char *fmt, ...)
{
    char buf[128];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    tap_note(buf);
}

void tap_begin_category(const char *name)
{
    strncpy(current_category, name, sizeof(current_category) - 1);
    current_category[sizeof(current_category) - 1] = '\0';

    cat_passed = 0;
    cat_failed = 0;
    cat_known = 0;
    cat_skipped = 0;
    cat_total = 0;
    cat_failure_count = 0;
    cat_note_count = 0;

    /* Log: category marker */
    log_printf("# --- %s ---\n", name);

    /* Screen: progress indicator (non-verbose only).
     * Shows category name with dots while tests run.
     * tap_end_category() rewrites this line with results. */
    if (!verbose) {
        print_cat_dots(name);
        fflush(stdout);
    }
}

void tap_end_category(void)
{
    int total_ran;
    int i;

    /* Accumulate into global screen counters */
    screen_passed += cat_passed;
    screen_failed += cat_failed;
    screen_known += cat_known;
    screen_skipped += cat_skipped;

    total_ran = cat_passed + cat_failed + cat_known;

    /* Rewrite progress indicator line (non-verbose), or start
     * fresh line after verbose individual test output */
    if (!verbose)
        putchar('\r');

    /* Category name with dot-padding */
    print_cat_dots(current_category);
    putchar(' ');

    /* N/M count + status */
    printf("%d/%d ", cat_passed, total_ran);

    if (cat_failed > 0)
        printf(CSI_BOLD "FAILED" CSI_RESET);
    else
        printf("passed");

    print_detail_suffix(cat_failed, cat_known, cat_skipped);
    printf("\n");
    page_check();

    /* Expand unexpected failures */
    for (i = 0; i < cat_failure_count; i++) {
        int line_len;

        line_len = printf("  FAIL #%d: %s\n",
                          cat_failures[i].test_num,
                          cat_failures[i].description);
        page_advance(wrap_rows(line_len > 1 ? line_len - 1 : 1));
    }
    if (cat_failed > cat_failure_count) {
        printf("  ... and %d more (see log)\n",
               cat_failed - cat_failure_count);
        page_check();
    }

    /* Show notable results */
    for (i = 0; i < cat_note_count; i++) {
        int line_len;

        line_len = printf("  %s\n", cat_notes[i]);
        page_advance(wrap_rows(line_len > 1 ? line_len - 1 : 1));
    }

    /* Clear category */
    current_category[0] = '\0';
}

void tap_bail(const char *reason)
{
    bailed_out = 1;
    printf("Bail out! %s\n", reason);
    page_check();
    log_printf("Bail out! %s\n", reason);
}

int tap_bailed(void)
{
    return bailed_out;
}

int tap_finish(void)
{
    int total_ran;
    int sum_passed, sum_failed, sum_known, sum_skipped;

    /* Use global counters if bail-out interrupted a category before
     * tap_end_category() could accumulate into screen counters. */
    if (bailed_out) {
        sum_passed = passed_count;
        sum_failed = failed_count;
        sum_known = known_count;
        sum_skipped = skipped_count;
    } else {
        sum_passed = screen_passed;
        sum_failed = screen_failed;
        sum_known = screen_known;
        sum_skipped = screen_skipped;
    }

    total_ran = sum_passed + sum_failed + sum_known;

    /* Screen: summary line (bold, always shown) */
    printf("\n");
    page_check();
    printf(CSI_BOLD "Results: %d/%d ", sum_passed, total_ran);

    if (sum_failed > 0)
        printf("FAILED");
    else
        printf("passed");

    print_detail_suffix(sum_failed, sum_known, sum_skipped);
    printf(CSI_RESET "\n");
    page_check();

    /* Log: summary diagnostic */
    log_printf("# Results: %d passed, %d failed, %d known, %d skipped"
               " (%d total)\n",
               sum_passed, sum_failed, sum_known, sum_skipped,
               test_number);

    if (logfp) {
        fclose(logfp);
        logfp = NULL;
    }

    if (bailed_out)
        return RETURN_FAIL;
    if (failed_count > 0)
        return RETURN_WARN;
    return RETURN_OK;
}

void tap_set_verbose(int flag)
{
    verbose = flag;
}

void tap_set_page(int flag)
{
    page_mode = flag;
    if (flag) {
        /* Silently disable if stdout or stdin is not a console */
        if (!IsInteractive(Output()) || !IsInteractive(Input())) {
            page_mode = 0;
            return;
        }
        screen_height = detect_screen_size();
        if (screen_height <= 0) {
            page_mode = 0;
            return;
        }
    }
}

int tap_get_total(void)
{
    return test_number;
}

int tap_get_passed(void)
{
    return passed_count;
}

int tap_get_failed(void)
{
    return failed_count;
}
