/*
 * bsdsocktest — TAP (Test Anything Protocol) v12 output framework
 */

#include "tap.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#include <dos/dos.h>

/* Internal state */
static int test_number;
static int passed_count;
static int failed_count;
static int bailed_out;
static int in_todo;
static const char *todo_reason;
static int verbose;
static FILE *logfp;

/* Write a line to both stdout and the optional log file. */
static void tap_puts(const char *line)
{
    puts(line);
    if (logfp) {
        fputs(line, logfp);
        fputc('\n', logfp);
    }
}

/* printf to both stdout and the optional log file. */
static void tap_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);

    if (logfp) {
        va_start(ap, fmt);
        vfprintf(logfp, fmt, ap);
        va_end(ap);
    }
}

void tap_init(const char *bsdlib_version)
{
    test_number = 0;
    passed_count = 0;
    failed_count = 0;
    bailed_out = 0;
    in_todo = 0;
    todo_reason = NULL;

    tap_puts("TAP version 12");
    tap_printf("# bsdsocktest %s\n", BSDSOCKTEST_VERSION);
    if (bsdlib_version)
        tap_printf("# bsdsocket.library: %s\n", bsdlib_version);
    else
        tap_puts("# bsdsocket.library: not available");
}

void tap_plan(int count)
{
    tap_printf("1..%d\n", count);
}

void tap_ok(int passed, const char *description)
{
    test_number++;

    if (in_todo) {
        /* TODO tests: report status but always count as "not a real failure" */
        if (passed) {
            tap_printf("ok %d - %s # TODO %s\n",
                       test_number, description, todo_reason);
            passed_count++;
        } else {
            tap_printf("not ok %d - %s # TODO %s\n",
                       test_number, description, todo_reason);
            /* TODO failures don't count against the pass rate */
            passed_count++;
        }
    } else {
        if (passed) {
            tap_printf("ok %d - %s\n", test_number, description);
            passed_count++;
        } else {
            tap_printf("not ok %d - %s\n", test_number, description);
            failed_count++;
        }
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
    test_number++;
    passed_count++;
    tap_printf("ok %d - # SKIP %s\n", test_number, reason);
}

void tap_todo_start(const char *reason)
{
    in_todo = 1;
    todo_reason = reason;
}

void tap_todo_end(void)
{
    in_todo = 0;
    todo_reason = NULL;
}

void tap_diag(const char *msg)
{
    tap_printf("# %s\n", msg);
}

void tap_diagf(const char *fmt, ...)
{
    char buf[256];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    tap_printf("# %s\n", buf);
}

void tap_bail(const char *reason)
{
    bailed_out = 1;
    tap_printf("Bail out! %s\n", reason);
}

int tap_finish(void)
{
    if (verbose || failed_count > 0) {
        tap_diagf("Tests: %d, Passed: %d, Failed: %d",
                  test_number, passed_count, failed_count);
    }

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

void tap_set_logfile(const char *path)
{
    if (logfp) {
        fclose(logfp);
        logfp = NULL;
    }

    logfp = fopen(path, "w");
    if (!logfp)
        tap_diagf("Warning: could not open log file %s", path);
}

void tap_set_verbose(int flag)
{
    verbose = flag;
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
