/*
 * bsdsocktest — TAP (Test Anything Protocol) v12 output framework
 *
 * Dual-output architecture: compact dashboard on screen (stdout),
 * full TAP v12 detail in a log file.
 */

#ifndef BSDSOCKTEST_TAP_H
#define BSDSOCKTEST_TAP_H

#define BSDSOCKTEST_VERSION "0.1.0"

/* Initialize TAP output. Opens log file and writes headers.
 * bsdlib_version: SBTC_RELEASESTRPTR string, or NULL if unavailable.
 * log_path: path for TAP log file. NULL defaults to "bsdsocktest.log".
 *           "NIL:" suppresses logging. */
void tap_init(const char *bsdlib_version, const char *log_path);

/* Emit the TAP plan line (written to log only). */
void tap_plan(int count);

/* Record a test result.
 * passed: non-zero for ok, zero for not ok.
 * description: test description string. */
void tap_ok(int passed, const char *description);

/* Record a test result with printf-style description. */
void tap_okf(int passed, const char *fmt, ...);

/* Skip a test with the given reason. */
void tap_skip(const char *reason);

/* Emit a diagnostic comment (log file only in compact mode). */
void tap_diag(const char *msg);

/* Emit a diagnostic comment with printf-style formatting (log only). */
void tap_diagf(const char *fmt, ...);

/* Emit a notable result visible on screen AND in the log.
 * On screen: appears indented under the category summary.
 * In the log: appears as a TAP diagnostic: "# <message>". */
void tap_note(const char *msg);

/* Emit a notable result with printf-style formatting. */
void tap_notef(const char *fmt, ...);

/* Set the active test category. Resets per-category counters.
 * Call before running each category's tests. */
void tap_begin_category(const char *name);

/* Finalize the active category. Emits category summary to screen. */
void tap_end_category(void);

/* Emit a TAP Bail out! line (both screen and log). */
void tap_bail(const char *reason);

/* Query whether a bail out has occurred. */
int tap_bailed(void);

/* Finalize TAP output. Emits summary to screen, closes log.
 * Returns AmigaOS exit code:
 * RETURN_OK (0) if all passed, RETURN_WARN (5) if any unexpected failures,
 * RETURN_FAIL (20) if bail out occurred. */
int tap_finish(void);

/* Enable/disable verbose mode. When verbose, individual test results
 * also appear on screen (not just category summaries). */
void tap_set_verbose(int flag);

/* Enable/disable pagination. When enabled, screen output pauses
 * after each screenful. Detects screen height automatically.
 * Silently disables if stdout/stdin is not a console. */
void tap_set_page(int flag);

/* Query counters. */
int tap_get_total(void);
int tap_get_passed(void);
int tap_get_failed(void);

#endif /* BSDSOCKTEST_TAP_H */
