/*
 * bsdsocktest — TAP (Test Anything Protocol) v12 output framework
 *
 * Emits TAP-compliant output to stdout, with optional duplication
 * to a log file (AmigaOS path, e.g. "RAM:test.log").
 */

#ifndef BSDSOCKTEST_TAP_H
#define BSDSOCKTEST_TAP_H

#define BSDSOCKTEST_VERSION "0.1.0"

/* Initialize TAP output. Emits version header and bsdsocktest version.
 * Call once at startup before any tests.
 * bsdlib_version may be NULL if bsdsocket.library is not available. */
void tap_init(const char *bsdlib_version);

/* Emit the TAP plan line: "1..N" */
void tap_plan(int count);

/* Record a test result.
 * passed: non-zero for ok, zero for not ok.
 * description: test description string. */
void tap_ok(int passed, const char *description);

/* Record a test result with printf-style description. */
void tap_okf(int passed, const char *fmt, ...);

/* Skip a test with the given reason. Counts as a pass. */
void tap_skip(const char *reason);

/* Mark subsequent tests as TODO (expected failures). */
void tap_todo_start(const char *reason);

/* End TODO block. */
void tap_todo_end(void);

/* Emit a diagnostic comment: "# message" */
void tap_diag(const char *msg);

/* Emit a diagnostic comment with printf-style formatting. */
void tap_diagf(const char *fmt, ...);

/* Emit a TAP Bail out! line and mark suite as fatally failed. */
void tap_bail(const char *reason);

/* Finalize TAP output. Returns AmigaOS exit code:
 * RETURN_OK (0) if all passed, RETURN_WARN (5) if any failed,
 * RETURN_FAIL (20) if bail out occurred. */
int tap_finish(void);

/* Set log file path (AmigaOS path). Output is duplicated to this file.
 * If the file cannot be opened, a diagnostic warning is emitted and
 * logging continues without the file. */
void tap_set_logfile(const char *path);

/* Enable/disable verbose mode. When verbose, diagnostics include
 * additional detail. */
void tap_set_verbose(int flag);

/* Query counters. */
int tap_get_total(void);
int tap_get_passed(void);
int tap_get_failed(void);

#endif /* BSDSOCKTEST_TAP_H */
