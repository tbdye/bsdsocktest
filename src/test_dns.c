/*
 * bsdsocktest — DNS/name resolution tests
 *
 * Tests: gethostbyname, gethostbyaddr, getservbyname, getservbyport,
 *        getprotobyname, getprotobynumber, gethostname, gethostid.
 *
 * 13 tests (100-112), port offsets 100-119.
 */

#include "tap.h"
#include "testutil.h"

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

    /* 100. gethostbyname_localhost */
    h = gethostbyname((STRPTR)"localhost");
    if (h) {
        struct in_addr resolved;
        memcpy(&resolved, h->h_addr_list[0], sizeof(resolved));
        tap_ok(h->h_addrtype == AF_INET &&
               h->h_length == 4 &&
               resolved.s_addr == htonl(INADDR_LOOPBACK),
               "gethostbyname_localhost - resolves to 127.0.0.1");
        tap_diagf("  resolved: %s", Inet_NtoA(resolved.s_addr));
    } else {
        tap_ok(0, "gethostbyname_localhost - returned NULL");
        tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
    }

    CHECK_CTRLC();

    /* 101. gethostbyname_invalid */
    h = gethostbyname((STRPTR)"nonexistent.invalid");
    tap_ok(h == NULL && get_bsd_h_errno() != 0,
           "gethostbyname_invalid - NULL with h_errno set for .invalid TLD");
    tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());

    CHECK_CTRLC();

    /* ---- gethostbyaddr ---- */

    /* 102. gethostbyaddr_loopback */
    addr.s_addr = htonl(INADDR_LOOPBACK);
    h = gethostbyaddr((STRPTR)&addr, sizeof(addr), AF_INET);
    if (h) {
        tap_ok(1, "gethostbyaddr_loopback - reverse lookup succeeded");
        tap_diagf("  hostname: %s", (const char *)h->h_name);
    } else {
        tap_ok(1, "gethostbyaddr_loopback - reverse DNS not configured for loopback");
        tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
    }

    CHECK_CTRLC();

    /* 103. gethostbyaddr_zero */
    addr.s_addr = 0;
    h = gethostbyaddr((STRPTR)&addr, sizeof(addr), AF_INET);
    if (h) {
        tap_ok(1, "gethostbyaddr_zero - 0.0.0.0 reverse lookup returned result");
        tap_diagf("  hostname: %s", (const char *)h->h_name);
    } else {
        tap_ok(1, "gethostbyaddr_zero - 0.0.0.0 reverse lookup returned NULL");
        tap_diagf("  h_errno=%ld", (long)get_bsd_h_errno());
    }

    CHECK_CTRLC();

    /* ---- getservbyname / getservbyport ---- */

    /* 104. getservbyname_http */
    s = getservbyname((STRPTR)"http", (STRPTR)"tcp");
    if (s) {
        tap_ok(ntohs(s->s_port) == 80,
               "getservbyname_http - http/tcp resolves to port 80");
        tap_diagf("  port=%d", ntohs(s->s_port));
    } else {
        tap_todo_start("services database does not include http");
        tap_ok(0, "getservbyname_http - returned NULL");
        tap_todo_end();
    }

    CHECK_CTRLC();

    /* 105. getservbyname_nonexistent */
    s = getservbyname((STRPTR)"nonexistent_service_xyz", (STRPTR)"tcp");
    tap_ok(s == NULL,
           "getservbyname_nonexistent - returns NULL for unknown service");

    CHECK_CTRLC();

    /* 106. getservbyport_80 */
    s = getservbyport(htons(80), (STRPTR)"tcp");
    if (s) {
        tap_ok(stricmp((const char *)s->s_name, "http") == 0,
               "getservbyport_80 - port 80/tcp resolves to http");
        tap_diagf("  name=%s", (const char *)s->s_name);
    } else {
        tap_todo_start("services database does not include port 80 lookup");
        tap_ok(0, "getservbyport_80 - returned NULL");
        tap_todo_end();
    }

    CHECK_CTRLC();

    /* ---- getprotobyname / getprotobynumber ---- */

    /* 107. getprotobyname_tcp */
    p = getprotobyname((STRPTR)"tcp");
    if (p) {
        tap_ok(p->p_proto == 6,
               "getprotobyname_tcp - tcp resolves to protocol 6");
        tap_diagf("  proto=%d", p->p_proto);
    } else {
        tap_todo_start("protocols database not available");
        tap_ok(0, "getprotobyname_tcp - returned NULL");
        tap_todo_end();
    }

    CHECK_CTRLC();

    /* 108. getprotobyname_udp */
    p = getprotobyname((STRPTR)"udp");
    if (p) {
        tap_ok(p->p_proto == 17,
               "getprotobyname_udp - udp resolves to protocol 17");
        tap_diagf("  proto=%d", p->p_proto);
    } else {
        tap_todo_start("protocols database not available");
        tap_ok(0, "getprotobyname_udp - returned NULL");
        tap_todo_end();
    }

    CHECK_CTRLC();

    /* 109. getprotobynumber_6 */
    p = getprotobynumber(6);
    if (p) {
        tap_ok(stricmp((const char *)p->p_name, "tcp") == 0,
               "getprotobynumber_6 - protocol 6 resolves to tcp");
        tap_diagf("  name=%s", (const char *)p->p_name);
    } else {
        tap_todo_start("protocols database not available");
        tap_ok(0, "getprotobynumber_6 - returned NULL");
        tap_todo_end();
    }

    CHECK_CTRLC();

    /* ---- gethostname / gethostid ---- */

    /* 110. gethostname_basic */
    memset(hostname, 0, sizeof(hostname));
    rc = gethostname((STRPTR)hostname, sizeof(hostname));
    tap_ok(rc == 0 && strlen(hostname) > 0,
           "gethostname_basic - returns non-empty hostname");
    tap_diagf("  rc=%d, hostname=\"%s\"", rc, hostname);

    CHECK_CTRLC();

    /* 111. gethostname_truncation */
    memset(small, 'X', sizeof(small));
    rc = gethostname((STRPTR)small, sizeof(small));
    if (rc == 0) {
        tap_ok(1, "gethostname_truncation - truncated without error");
        tap_diagf("  small[0]=0x%02x small[1]=0x%02x",
                  (unsigned char)small[0], (unsigned char)small[1]);
    } else {
        tap_ok(1, "gethostname_truncation - returns error on small buffer");
        tap_diagf("  rc=%d, errno=%ld", rc, (long)get_bsd_errno());
    }

    CHECK_CTRLC();

    /* 112. gethostid_nonzero */
    hostid = gethostid();
    tap_ok(hostid != 0,
           "gethostid_nonzero - returns non-zero host ID");
    tap_diagf("  gethostid=0x%08lx", (unsigned long)hostid);
}
