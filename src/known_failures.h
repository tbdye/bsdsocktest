/*
 * bsdsocktest — Known failure detection per TCP/IP stack
 *
 * Auto-detects the running stack from SBTC_RELEASESTRPTR and looks up
 * known test failures. Tests report pass/fail normally; the framework
 * marks matching failures as "known" rather than "unexpected".
 */

#ifndef KNOWN_FAILURES_H
#define KNOWN_FAILURES_H

/* Initialize known-failures table from the detected stack version string.
 * Call once after opening bsdsocket.library, before running any tests.
 * version_string: the SBTC_RELEASESTRPTR value (e.g. "Roadshow 4.364").
 * Matched exactly against known profiles; unrecognized stacks get no
 * annotations. */
void known_init(const char *version_string);

/* Check if a given test number is a known issue (failure or crash)
 * for the current stack and version. Returns the reason string if
 * known, NULL if not. Used by the TAP framework to annotate output. */
const char *known_check(int test_number);

/* Check if a given test number would crash the current stack.
 * Returns the reason string if so, NULL if safe to run.
 * Test code calls this before exercising crash-prone operations.
 * When non-NULL, emit tap_ok(0, desc) + diagnostic and skip the test. */
const char *known_crash(int test_number);

/* Get the detected stack name (e.g. "Roadshow"), or "Unknown" if
 * not recognized. For display purposes. */
const char *known_stack_name(void);

#endif /* KNOWN_FAILURES_H */
