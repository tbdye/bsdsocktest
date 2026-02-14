/*
 * bsdsocktest — Miscellaneous tests
 *
 * Tests: getdtablesize, syslog, CloseSocket after shutdown,
 *        open max sockets.
 *
 * 5 tests (118-122), port offsets 140-159.
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>

#include <netinet/in.h>
#include <string.h>

/* LOG_INFO from <sys/syslog.h> — defined directly because <sys/syslog.h>
 * conflicts with the bsdsocket inline syslog macro from proto/bsdsocket.h */
#ifndef LOG_INFO
#define LOG_INFO 6
#endif

void run_misc_tests(void)
{
    static LONG fds[256];
    LONG listener, client, server;
    LONG dtsize, new_dtsize, orig_dtsize;
    int port, rc, i, count;

    /* ---- getdtablesize ---- */

    /* 118. getdtablesize_default */
    dtsize = getdtablesize();
    tap_ok(dtsize >= 64,
           "getdtablesize_default - returns >= 64");
    tap_diagf("  dtablesize=%ld", (long)dtsize);

    CHECK_CTRLC();

    /* 119. getdtablesize_after_set */
    orig_dtsize = 0;
    SocketBaseTags(SBTM_GETREF(SBTC_DTABLESIZE), (ULONG)&orig_dtsize,
                   TAG_DONE);
    new_dtsize = orig_dtsize + 64;
    SocketBaseTags(SBTM_SETVAL(SBTC_DTABLESIZE), new_dtsize, TAG_DONE);
    dtsize = getdtablesize();
    tap_ok(dtsize >= new_dtsize,
           "getdtablesize_after_set - reflects expanded value");
    tap_diagf("  before=%ld, requested=%ld, getdtablesize=%ld",
              (long)orig_dtsize, (long)new_dtsize, (long)dtsize);
    /* Restore (may not reduce) */
    if (orig_dtsize > 0)
        SocketBaseTags(SBTM_SETVAL(SBTC_DTABLESIZE), orig_dtsize, TAG_DONE);

    CHECK_CTRLC();

    /* ---- syslog ---- */

    /* 120. syslog_no_crash */
    /* The syslog() convenience macro is broken in this SDK version
     * (_sfdc_vararg undefined). Call vsyslog directly with a manual
     * argument array matching AmigaOS varargs convention (ULONG[]). */
    {
        ULONG syslog_args[1];
        syslog_args[0] = (ULONG)(STRPTR)"test";
        SocketBaseTags(SBTM_SETVAL(SBTC_LOGTAGPTR),
                       (ULONG)(STRPTR)"bsdsocktest", TAG_DONE);
        vsyslog(LOG_INFO, (STRPTR)"phase 4 canary %s", (APTR)syslog_args);
    }
    tap_ok(1, "syslog_no_crash - syslog canary (no crash)");

    CHECK_CTRLC();

    /* ---- CloseSocket after shutdown ---- */

    /* 121. closesocket_after_shutdown */
    port = get_test_port(140);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        shutdown(client, 2); /* SHUT_RDWR */
        rc = CloseSocket(client);
        client = -1;
        tap_ok(rc == 0,
               "closesocket_after_shutdown - CloseSocket returns 0 after shutdown");
        tap_diagf("  rc=%d", rc);
    } else {
        tap_ok(0, "closesocket_after_shutdown - could not establish connection");
    }
    safe_close(client);
    safe_close(server);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Open max sockets ---- */

    /* 122. open_max_sockets */
    dtsize = getdtablesize();
    for (i = 0; i < 256; i++)
        fds[i] = -1;

    count = 0;
    for (i = 0; i < dtsize - 1 && i < 256; i++) {
        fds[i] = make_tcp_socket();
        if (fds[i] < 0)
            break;
        count++;
    }
    tap_ok(count >= 32,
           "open_max_sockets - opened >= 32 simultaneous sockets");
    tap_diagf("  opened=%d, dtablesize=%ld", count, (long)dtsize);

    /* Close in reverse order */
    for (i = count - 1; i >= 0; i--) {
        safe_close(fds[i]);
        fds[i] = -1;
    }
}
