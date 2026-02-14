/*
 * bsdsocktest — Socket option tests
 *
 * Tests: getsockopt, setsockopt (SO_TYPE, SO_REUSEADDR, SO_KEEPALIVE,
 *        SO_LINGER, SO_RCVTIMEO, SO_SNDTIMEO, TCP_NODELAY, SO_ERROR,
 *        SO_RCVBUF, SO_SNDBUF), IoctlSocket (FIONBIO, FIONREAD, FIOASYNC).
 *
 * Phase 1: stub only.
 */

#include "tap.h"
#include "testutil.h"

void run_sockopt_tests(void)
{
    tap_diag("(not yet implemented)");
}
