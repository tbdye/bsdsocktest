/*
 * bsdsocktest — Utility function tests
 *
 * Tests: Inet_NtoA, inet_addr, Inet_LnaOf, Inet_NetOf,
 *        Inet_MakeAddr, inet_network.
 *
 * 10 tests (105-114), no ports needed.
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

    /* 105. inet_ntoa_loopback */
    result = (const char *)Inet_NtoA(htonl(0x7f000001));
    tap_ok(result && strcmp(result, "127.0.0.1") == 0,
           "Inet_NtoA(): 127.0.0.1 formatting [AmiTCP]");
    if (result)
        tap_diagf("  returned: \"%s\"", result);

    CHECK_CTRLC();

    /* 106. inet_ntoa_broadcast */
    result = (const char *)Inet_NtoA(htonl(0xffffffff));
    tap_ok(result && strcmp(result, "255.255.255.255") == 0,
           "Inet_NtoA(): 255.255.255.255 formatting [AmiTCP]");
    if (result)
        tap_diagf("  returned: \"%s\"", result);

    CHECK_CTRLC();

    /* 107. inet_ntoa_zero */
    result = (const char *)Inet_NtoA(0);
    tap_ok(result && strcmp(result, "0.0.0.0") == 0,
           "Inet_NtoA(): 0.0.0.0 formatting [AmiTCP]");
    if (result)
        tap_diagf("  returned: \"%s\"", result);

    CHECK_CTRLC();

    /* ---- inet_addr ---- */

    /* 108. inet_addr_valid */
    addr_val = inet_addr((STRPTR)"127.0.0.1");
    tap_ok(addr_val == htonl(0x7f000001),
           "inet_addr(): parse \"127.0.0.1\" [BSD 4.4]");

    CHECK_CTRLC();

    /* 109. inet_addr_invalid */
    addr_val = inet_addr((STRPTR)"not.an.ip");
    tap_ok(addr_val == INADDR_NONE,
           "inet_addr(): invalid string returns INADDR_NONE [BSD 4.4]");

    CHECK_CTRLC();

    /* 110. inet_addr_broadcast */
    addr_val = inet_addr((STRPTR)"255.255.255.255");
    tap_ok(addr_val == 0xffffffff,
           "inet_addr(): \"255.255.255.255\" [BSD 4.4]");
    tap_diag("  note: INADDR_NONE ambiguity with broadcast address");

    CHECK_CTRLC();

    /* ---- Inet_LnaOf / Inet_NetOf / Inet_MakeAddr ---- */

    /* 111. inet_lnaof — Class A (10.x.x.x), host part is 0x010203 */
    host = Inet_LnaOf(htonl(0x0a010203));
    tap_ok(host == 0x010203,
           "Inet_LnaOf(): extract host part [AmiTCP]");
    tap_diagf("  host part: 0x%06lx (expected 0x010203)", (unsigned long)host);

    CHECK_CTRLC();

    /* 112. inet_netof — Class A (10.x.x.x), network part is 0x0a */
    net = Inet_NetOf(htonl(0x0a010203));
    tap_ok(net == 0x0a,
           "Inet_NetOf(): extract network part [AmiTCP]");
    tap_diagf("  net part: 0x%02lx (expected 0x0a)", (unsigned long)net);

    CHECK_CTRLC();

    /* 113. inet_makeaddr_roundtrip */
    net = Inet_NetOf(htonl(0x0a010203));
    host = Inet_LnaOf(htonl(0x0a010203));
    rebuilt = Inet_MakeAddr(net, host);
    tap_ok(rebuilt == htonl(0x0a010203),
           "Inet_MakeAddr(): round-trip with LnaOf/NetOf [AmiTCP]");
    tap_diagf("  rebuilt: 0x%08lx (expected 0x%08lx)",
              (unsigned long)rebuilt, (unsigned long)htonl(0x0a010203));

    CHECK_CTRLC();

    /* ---- inet_network ---- */

    /* 114. inet_network */
    addr_val = inet_network((STRPTR)"10.0.0.0");
    tap_ok(addr_val == 0x0a000000,
           "inet_network(): host byte order conversion [BSD 4.4]");
    tap_diagf("  returned: 0x%08lx (expected 0x0a000000)", (unsigned long)addr_val);
}
