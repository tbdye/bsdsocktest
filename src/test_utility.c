/*
 * bsdsocktest — Utility function tests
 *
 * Tests: Inet_NtoA, inet_addr, Inet_LnaOf, Inet_NetOf,
 *        Inet_MakeAddr, inet_network.
 *
 * Phase 1: smoke test only (Inet_NtoA).
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>
#include <netinet/in.h>

#include <string.h>

void run_utility_tests(void)
{
    const char *result;

    /* Smoke test: Inet_NtoA(0x7f000001) should return "127.0.0.1" */
    result = (const char *)Inet_NtoA(htonl(0x7f000001));
    tap_ok(result && strcmp(result, "127.0.0.1") == 0,
           "Inet_NtoA(127.0.0.1) returns correct string");

    if (result)
        tap_diagf("  Inet_NtoA returned: \"%s\"", result);
    else
        tap_diag("  Inet_NtoA returned NULL");
}
