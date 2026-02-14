/*
 * bsdsocktest — WaitSelect tests
 *
 * Tests: read/write readiness, timeout, NULL fdsets, exceptfds,
 *        signal interruption, nfds boundary, >64 descriptors,
 *        connect readiness, peer close readiness.
 *
 * Phase 1: stub only.
 */

#include "tap.h"
#include "testutil.h"

void run_waitselect_tests(void)
{
    tap_diag("(not yet implemented)");
}
