/*
 * bsdsocktest — Utility function tests
 *
 * Tests: Inet_NtoA, inet_addr, Inet_LnaOf, Inet_NetOf,
 *        Inet_MakeAddr, inet_network.
 *
 * 10 tests, no ports needed.
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>
#include <netinet/in.h>

#include <string.h>

void run_utility_tests(void)
{
    const char *result;
    in_addr_t addr_val, net, host, rebuilt;

    /* ---- Inet_NtoA ---- */

    /* 60. inet_ntoa_loopback */
    result = (const char *)Inet_NtoA(htonl(0x7f000001));
    tap_ok(result && strcmp(result, "127.0.0.1") == 0,
           "inet_ntoa_loopback - Inet_NtoA(127.0.0.1)");
    if (result)
        tap_diagf("  returned: \"%s\"", result);

    CHECK_CTRLC();

    /* 61. inet_ntoa_broadcast */
    result = (const char *)Inet_NtoA(htonl(0xffffffff));
    tap_ok(result && strcmp(result, "255.255.255.255") == 0,
           "inet_ntoa_broadcast - Inet_NtoA(255.255.255.255)");
    if (result)
        tap_diagf("  returned: \"%s\"", result);

    CHECK_CTRLC();

    /* 62. inet_ntoa_zero */
    result = (const char *)Inet_NtoA(0);
    tap_ok(result && strcmp(result, "0.0.0.0") == 0,
           "inet_ntoa_zero - Inet_NtoA(0.0.0.0)");
    if (result)
        tap_diagf("  returned: \"%s\"", result);

    CHECK_CTRLC();

    /* ---- inet_addr ---- */

    /* 63. inet_addr_valid */
    addr_val = inet_addr((STRPTR)"127.0.0.1");
    tap_ok(addr_val == htonl(0x7f000001),
           "inet_addr_valid - inet_addr(\"127.0.0.1\")");

    CHECK_CTRLC();

    /* 64. inet_addr_invalid */
    addr_val = inet_addr((STRPTR)"not.an.ip");
    tap_ok(addr_val == INADDR_NONE,
           "inet_addr_invalid - inet_addr(\"not.an.ip\") returns INADDR_NONE");

    CHECK_CTRLC();

    /* 65. inet_addr_broadcast */
    addr_val = inet_addr((STRPTR)"255.255.255.255");
    tap_ok(addr_val == 0xffffffff,
           "inet_addr_broadcast - inet_addr(\"255.255.255.255\") returns 0xffffffff");
    tap_diag("  note: INADDR_NONE ambiguity with broadcast address");

    CHECK_CTRLC();

    /* ---- Inet_LnaOf / Inet_NetOf / Inet_MakeAddr ---- */

    /* 66. inet_lnaof — Class A (10.x.x.x), host part is 0x010203 */
    host = Inet_LnaOf(htonl(0x0a010203));
    tap_ok(host == 0x010203,
           "inet_lnaof - Inet_LnaOf(10.1.2.3) returns host part");
    tap_diagf("  host part: 0x%06lx (expected 0x010203)", (unsigned long)host);

    CHECK_CTRLC();

    /* 67. inet_netof — Class A (10.x.x.x), network part is 0x0a */
    net = Inet_NetOf(htonl(0x0a010203));
    tap_ok(net == 0x0a,
           "inet_netof - Inet_NetOf(10.1.2.3) returns network part");
    tap_diagf("  net part: 0x%02lx (expected 0x0a)", (unsigned long)net);

    CHECK_CTRLC();

    /* 68. inet_makeaddr_roundtrip */
    net = Inet_NetOf(htonl(0x0a010203));
    host = Inet_LnaOf(htonl(0x0a010203));
    rebuilt = Inet_MakeAddr(net, host);
    tap_ok(rebuilt == htonl(0x0a010203),
           "inet_makeaddr_roundtrip - MakeAddr(NetOf, LnaOf) round-trips");
    tap_diagf("  rebuilt: 0x%08lx (expected 0x%08lx)",
              (unsigned long)rebuilt, (unsigned long)htonl(0x0a010203));

    CHECK_CTRLC();

    /* ---- inet_network ---- */

    /* 69. inet_network */
    addr_val = inet_network((STRPTR)"10.0.0.0");
    tap_ok(addr_val == 0x0a000000,
           "inet_network - inet_network(\"10.0.0.0\") returns host byte order");
    tap_diagf("  returned: 0x%08lx (expected 0x0a000000)", (unsigned long)addr_val);
}
