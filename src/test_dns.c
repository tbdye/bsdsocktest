/*
 * bsdsocktest — DNS/name resolution tests
 *
 * Tests: gethostbyname, gethostbyaddr, getservbyname, getservbyport,
 *        getprotobyname, getprotobynumber, gethostname, gethostid.
 *
 * 17 tests (88-104).
 */

#include "tap.h"
#include "testutil.h"
#include "helper_proto.h"

#include <proto/bsdsocket.h>

#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

void run_dns_tests(void)
{
    struct hostent *h;
    struct servent *s;
    struct protoent *p;
    struct in_addr addr;
    char hostname[256];
    char small[2];
    ULONG hostid;
    int rc;

    /* ---- gethostbyname ---- */

    /* 88. gethostbyname_localhost */
    h = gethostbyname((STRPTR)"localhost");
    if (h) {
        struct in_addr resolved;
        memcpy(&resolved, h->h_addr_list[0], sizeof(resolved));
        tap_ok(h->h_addrtype == AF_INET &&
               h->h_length == 4 &&
               resolved.s_addr == htonl(INADDR_LOOPBACK),
               "gethostbyname(): \"localhost\" resolves to 127.0.0.1 [BSD 4.4]");
        tap_diagf("  resolved: %s", Inet_NtoA(resolved.s_addr));
    } else {
        tap_ok(0, "gethostbyname(): \"localhost\" resolves to 127.0.0.1 [BSD 4.4]");
        tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
    }

    CHECK_CTRLC();

    /* 89. gethostbyname_invalid */
    h = gethostbyname((STRPTR)"nonexistent.invalid");
    tap_ok(h == NULL && get_bsd_h_errno() != 0,
           "gethostbyname(): invalid hostname sets h_errno [BSD 4.4]");
    tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());

    CHECK_CTRLC();

    /* ---- gethostbyaddr ---- */

    /* 90. gethostbyaddr_loopback */
    addr.s_addr = htonl(INADDR_LOOPBACK);
    h = gethostbyaddr((STRPTR)&addr, sizeof(addr), AF_INET);
    if (h) {
        tap_ok(1, "gethostbyaddr(): reverse lookup 127.0.0.1 [BSD 4.4]");
        tap_diagf("  hostname: %s", (const char *)h->h_name);
    } else {
        tap_ok(1, "gethostbyaddr(): reverse lookup 127.0.0.1 [BSD 4.4]");
        tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
    }

    CHECK_CTRLC();

    /* 91. gethostbyaddr_zero */
    addr.s_addr = 0;
    h = gethostbyaddr((STRPTR)&addr, sizeof(addr), AF_INET);
    if (h) {
        tap_ok(1, "gethostbyaddr(): 0.0.0.0 behavior [BSD 4.4]");
        tap_diagf("  hostname: %s", (const char *)h->h_name);
    } else {
        tap_ok(1, "gethostbyaddr(): 0.0.0.0 behavior [BSD 4.4]");
        tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
    }

    CHECK_CTRLC();

    /* ---- getservbyname / getservbyport ---- */

    /* 92. getservbyname_http */
    s = getservbyname((STRPTR)"http", (STRPTR)"tcp");
    if (s) {
        tap_ok(ntohs(s->s_port) == 80,
               "getservbyname(): \"http\"/\"tcp\" -> port 80 [BSD 4.4]");
        tap_diagf("  port=%d", ntohs(s->s_port));
    } else {
        tap_skip("services database does not include http");
    }

    CHECK_CTRLC();

    /* 93. getservbyname_nonexistent */
    s = getservbyname((STRPTR)"nonexistent_service_xyz", (STRPTR)"tcp");
    tap_ok(s == NULL,
           "getservbyname(): unknown service returns NULL [BSD 4.4]");

    CHECK_CTRLC();

    /* 94. getservbyport_80 */
    s = getservbyport(htons(80), (STRPTR)"tcp");
    if (s) {
        tap_ok(stricmp((const char *)s->s_name, "http") == 0,
               "getservbyport(): port 80/\"tcp\" -> \"http\" [BSD 4.4]");
        tap_diagf("  name=%s", (const char *)s->s_name);
    } else {
        tap_skip("services database does not include port 80");
    }

    CHECK_CTRLC();

    /* ---- getprotobyname / getprotobynumber ---- */

    /* 95. getprotobyname_tcp */
    p = getprotobyname((STRPTR)"tcp");
    if (p) {
        tap_ok(p->p_proto == 6,
               "getprotobyname(): \"tcp\" -> protocol 6 [BSD 4.4]");
        tap_diagf("  proto=%d", p->p_proto);
    } else {
        tap_skip("protocols database not available");
    }

    CHECK_CTRLC();

    /* 96. getprotobyname_udp */
    p = getprotobyname((STRPTR)"udp");
    if (p) {
        tap_ok(p->p_proto == 17,
               "getprotobyname(): \"udp\" -> protocol 17 [BSD 4.4]");
        tap_diagf("  proto=%d", p->p_proto);
    } else {
        tap_skip("protocols database not available");
    }

    CHECK_CTRLC();

    /* 97. getprotobynumber_6 */
    p = getprotobynumber(6);
    if (p) {
        tap_ok(stricmp((const char *)p->p_name, "tcp") == 0,
               "getprotobynumber(): 6 -> \"tcp\" [BSD 4.4]");
        tap_diagf("  name=%s", (const char *)p->p_name);
    } else {
        tap_skip("protocols database not available");
    }

    CHECK_CTRLC();

    /* ---- gethostname / gethostid ---- */

    /* 98. gethostname_basic */
    memset(hostname, 0, sizeof(hostname));
    rc = gethostname((STRPTR)hostname, sizeof(hostname));
    tap_ok(rc == 0 && strlen(hostname) > 0,
           "gethostname(): retrieve hostname [BSD 4.4]");
    tap_diagf("  rc=%d, hostname=\"%s\"", rc, hostname);

    CHECK_CTRLC();

    /* 99. gethostname_truncation */
    memset(small, 'X', sizeof(small));
    rc = gethostname((STRPTR)small, sizeof(small));
    if (rc == 0) {
        tap_ok(1, "gethostname(): small buffer truncation [BSD 4.4]");
        tap_diagf("  small[0]=0x%02x small[1]=0x%02x",
                  (unsigned char)small[0], (unsigned char)small[1]);
    } else {
        tap_ok(1, "gethostname(): small buffer truncation [BSD 4.4]");
        tap_diagf("  rc=%d, errno=%ld", rc, (long)get_bsd_errno());
    }

    CHECK_CTRLC();

    /* 100. gethostid_nonzero */
    hostid = gethostid();
    tap_ok(hostid != 0,
           "gethostid(): returns non-zero value [BSD 4.4]");
    tap_diagf("  gethostid=0x%08lx", (unsigned long)hostid);

    CHECK_CTRLC();

    /* ---- getnetbyname / getnetbyaddr ---- */

    /* 101. getnetbyname_loopback */
    {
        struct netent *n;

        n = getnetbyname((STRPTR)"loopback");
        if (n) {
            tap_ok(n->n_addrtype == AF_INET && n->n_net == 127,
                   "getnetbyname(): network database lookup [BSD 4.4]");
            tap_diagf("  n_name=%s n_net=%ld",
                      (const char *)n->n_name, (long)n->n_net);
        } else {
            tap_skip("networks database not available");
        }
    }

    CHECK_CTRLC();

    /* 102. getnetbyaddr_loopback */
    {
        struct netent *n;

        n = getnetbyaddr(127, AF_INET);
        if (n) {
            tap_ok(n->n_net == 127 && n->n_name != NULL &&
                   strlen((const char *)n->n_name) > 0,
                   "getnetbyaddr(): network reverse lookup [BSD 4.4]");
            tap_diagf("  n_name=%s n_net=%ld",
                      (const char *)n->n_name, (long)n->n_net);
        } else {
            tap_skip("networks database not available");
        }
    }

    CHECK_CTRLC();

    /* Network DNS tests — require host helper */
    if (!helper_is_connected()) {
        tap_skip("host helper not connected");
        CHECK_CTRLC();
        tap_skip("host helper not connected");
        return;
    }

    /* 103. gethostbyname_external */
    h = gethostbyname((STRPTR)"aminet.net");
    if (h) {
        struct in_addr resolved;
        memcpy(&resolved, h->h_addr_list[0], sizeof(resolved));
        tap_ok(h->h_addrtype == AF_INET && h->h_length == 4,
               "gethostbyname(): external hostname resolution [BSD 4.4]");
        tap_diagf("  resolved: %s", Inet_NtoA(resolved.s_addr));
    } else {
        tap_ok(0, "gethostbyname(): external hostname resolution [BSD 4.4]");
        tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
    }

    CHECK_CTRLC();

    /* 104. gethostbyaddr_external */
    {
        struct in_addr ext_addr;

        ext_addr.s_addr = helper_addr();
        h = gethostbyaddr((STRPTR)&ext_addr, sizeof(ext_addr), AF_INET);
        if (h) {
            tap_ok(h->h_addrtype == AF_INET && h->h_length == 4,
                   "gethostbyaddr(): external reverse lookup [BSD 4.4]");
            tap_diagf("  hostname: %s", (const char *)h->h_name);
        } else {
            tap_ok(1, "gethostbyaddr(): external reverse lookup [BSD 4.4]");
            tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
        }
    }
}
