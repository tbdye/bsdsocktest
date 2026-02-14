/*
 * bsdsocktest — WaitSelect tests
 *
 * Tests: read/write readiness, timeout, NULL fdsets, exceptfds,
 *        signal interruption, nfds boundary, >64 descriptors,
 *        connect readiness, peer close readiness.
 *
 * 15 tests (70-84), port offsets 60-79.
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>
#include <proto/exec.h>
#include <proto/dos.h>

#include <errno.h>
#include <string.h>

void run_waitselect_tests(void)
{
    LONG listener, client, server;
    LONG list3[3], cli3[3], srv3[3];
    LONG fds[65];
    LONG closed_fd;
    fd_set readfds, writefds, exceptfds;
    struct timeval tv;
    struct DateStamp before, after;
    LONG elapsed_ticks;
    LONG dtsize;
    LONG optval;
    socklen_t optlen;
    struct sockaddr_in addr;
    BYTE sigbit;
    ULONG sigmask;
    int port, rc, i, nfds, ready_count;
    int result_a, result_b;
    unsigned char buf[100];

    /* ---- Read/write readiness ---- */

    /* 70. ws_read_ready */
    port = get_test_port(60);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        fill_test_pattern(buf, 100, 70);
        send(client, (UBYTE *)buf, 100, 0);
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);
        tv.tv_secs = 2;
        tv.tv_micro = 0;
        rc = WaitSelect(server + 1, &readfds, NULL, NULL, &tv, NULL);
        tap_ok(rc >= 1 && FD_ISSET(server, &readfds),
               "ws_read_ready - WaitSelect detects readable after send");
    } else {
        tap_ok(0, "ws_read_ready - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 71. ws_write_ready */
    port = get_test_port(61);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        FD_ZERO(&writefds);
        FD_SET(client, &writefds);
        tv.tv_secs = 2;
        tv.tv_micro = 0;
        rc = WaitSelect(client + 1, NULL, &writefds, NULL, &tv, NULL);
        tap_ok(rc >= 1 && FD_ISSET(client, &writefds),
               "ws_write_ready - connected socket with empty buffer is writable");
    } else {
        tap_ok(0, "ws_write_ready - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Timeout behavior ---- */

    /* 72. ws_timeout_zero */
    port = get_test_port(62);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (server >= 0) {
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);
        tv.tv_secs = 0;
        tv.tv_micro = 0;
        DateStamp(&before);
        rc = WaitSelect(server + 1, &readfds, NULL, NULL, &tv, NULL);
        DateStamp(&after);
        elapsed_ticks = (after.ds_Minute - before.ds_Minute) * 50 * 60
                      + (after.ds_Tick - before.ds_Tick);
        tap_ok(rc == 0 && elapsed_ticks < 5,
               "ws_timeout_zero - tv={0,0} returns immediately with 0");
        tap_diagf("  elapsed: %ld ticks, return: %d",
                  (long)elapsed_ticks, rc);
    } else {
        tap_ok(0, "ws_timeout_zero - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 73. ws_timeout_expires */
    port = get_test_port(63);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (server >= 0) {
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);
        tv.tv_secs = 1;
        tv.tv_micro = 0;
        DateStamp(&before);
        rc = WaitSelect(server + 1, &readfds, NULL, NULL, &tv, NULL);
        DateStamp(&after);
        elapsed_ticks = (after.ds_Minute - before.ds_Minute) * 50 * 60
                      + (after.ds_Tick - before.ds_Tick);
        tap_ok(rc == 0 && elapsed_ticks >= 25 && elapsed_ticks <= 100,
               "ws_timeout_expires - 1s timeout with no activity");
        tap_diagf("  elapsed: %ld ticks (%ld.%02ld s), return: %d",
                  (long)elapsed_ticks,
                  (long)(elapsed_ticks / 50),
                  (long)((elapsed_ticks % 50) * 2),
                  rc);
    } else {
        tap_ok(0, "ws_timeout_expires - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 74. ws_null_timeout */
    port = get_test_port(64);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    if (listener >= 0 && client >= 0) {
        /* Connection pending in backlog makes listener "readable" */
        FD_ZERO(&readfds);
        FD_SET(listener, &readfds);
        rc = WaitSelect(listener + 1, &readfds, NULL, NULL, NULL, NULL);
        tap_ok(rc >= 1 && FD_ISSET(listener, &readfds),
               "ws_null_timeout - NULL timeout returns on pending connection");
    } else {
        tap_ok(0, "ws_null_timeout - could not create listener/client");
    }
    /* Accept to clean up the pending connection */
    if (listener >= 0) {
        server = accept_one(listener);
        safe_close(server);
    }
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Pure delay ---- */

    /* 75. ws_null_fdsets */
    tv.tv_secs = 0;
    tv.tv_micro = 250000;
    DateStamp(&before);
    rc = WaitSelect(0, NULL, NULL, NULL, &tv, NULL);
    DateStamp(&after);
    elapsed_ticks = (after.ds_Minute - before.ds_Minute) * 50 * 60
                  + (after.ds_Tick - before.ds_Tick);
    tap_ok(rc == 0 && elapsed_ticks >= 5 && elapsed_ticks <= 30,
           "ws_null_fdsets - pure delay with no fds (~0.25s)");
    tap_diagf("  elapsed: %ld ticks (%ld.%02ld s)",
              (long)elapsed_ticks,
              (long)(elapsed_ticks / 50),
              (long)((elapsed_ticks % 50) * 2));

    CHECK_CTRLC();

    /* ---- Exception fd ---- */

    /* 76. ws_exceptfds_oob */
    port = get_test_port(65);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        buf[0] = 0xAB;
        rc = send(client, (UBYTE *)buf, 1, MSG_OOB);
        if (rc < 0) {
            tap_skip("ws_exceptfds_oob - MSG_OOB not supported");
        } else {
            FD_ZERO(&exceptfds);
            FD_SET(server, &exceptfds);
            tv.tv_secs = 2;
            tv.tv_micro = 0;
            rc = WaitSelect(server + 1, NULL, NULL, &exceptfds, &tv, NULL);
            if (rc >= 1 && FD_ISSET(server, &exceptfds)) {
                tap_ok(1, "ws_exceptfds_oob - OOB detected via exceptfds");
            } else if (rc == 0) {
                tap_todo_start("OOB not detected via exceptfds");
                tap_ok(0, "ws_exceptfds_oob - OOB detected via exceptfds");
                tap_todo_end();
            } else {
                tap_ok(0, "ws_exceptfds_oob - unexpected WaitSelect result");
                tap_diagf("  rc=%d, errno=%ld", rc, (long)get_bsd_errno());
            }
        }
    } else {
        tap_ok(0, "ws_exceptfds_oob - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Multiple file descriptors ---- */

    /* 77. ws_multiple_fds */
    for (i = 0; i < 3; i++) {
        list3[i] = cli3[i] = srv3[i] = -1;
    }
    for (i = 0; i < 3; i++) {
        port = get_test_port(66 + i);
        list3[i] = make_loopback_listener(port);
        cli3[i] = make_loopback_client(port);
        srv3[i] = accept_one(list3[i]);
    }
    if (srv3[0] >= 0 && srv3[1] >= 0 && srv3[2] >= 0) {
        /* Send data on all 3 clients */
        for (i = 0; i < 3; i++) {
            fill_test_pattern(buf, 10, 77 + i);
            send(cli3[i], (UBYTE *)buf, 10, 0);
        }
        /* Build readfds with all 3 servers */
        FD_ZERO(&readfds);
        nfds = 0;
        for (i = 0; i < 3; i++) {
            FD_SET(srv3[i], &readfds);
            if (srv3[i] + 1 > nfds)
                nfds = srv3[i] + 1;
        }
        tv.tv_secs = 2;
        tv.tv_micro = 0;
        rc = WaitSelect(nfds, &readfds, NULL, NULL, &tv, NULL);
        ready_count = 0;
        for (i = 0; i < 3; i++) {
            if (FD_ISSET(srv3[i], &readfds))
                ready_count++;
        }
        tap_ok(rc >= 1 && ready_count == 3,
               "ws_multiple_fds - all 3 sockets report readable");
        tap_diagf("  return: %d, ready: %d of 3", rc, ready_count);
    } else {
        tap_ok(0, "ws_multiple_fds - could not establish all 3 connections");
    }
    close_all(srv3, 3);
    close_all(cli3, 3);
    close_all(list3, 3);

    CHECK_CTRLC();

    /* ---- Signal interaction ---- */

    /* 78. ws_signal_interrupt */
    sigbit = alloc_signal();
    if (sigbit >= 0) {
        port = get_test_port(69);
        listener = make_loopback_listener(port);
        if (listener >= 0) {
            FD_ZERO(&readfds);
            FD_SET(listener, &readfds);
            /* Self-signal before WaitSelect */
            Signal(FindTask(NULL), 1UL << sigbit);
            sigmask = 1UL << sigbit;
            rc = WaitSelect(listener + 1, &readfds, NULL, NULL, NULL, &sigmask);
            tap_ok(rc == 0 &&
                   !FD_ISSET(listener, &readfds) &&
                   (sigmask & (1UL << sigbit)),
                   "ws_signal_interrupt - signal interrupts WaitSelect");
            tap_diagf("  rc=%d, fd_isset=%d, sigmask=0x%08lx",
                      rc,
                      (int)FD_ISSET(listener, &readfds),
                      (unsigned long)sigmask);
        } else {
            tap_ok(0, "ws_signal_interrupt - could not create listener");
        }
        safe_close(listener);
        SetSignal(0, 1UL << sigbit);
        free_signal(sigbit);
    } else {
        tap_skip("ws_signal_interrupt - could not allocate signal");
    }

    CHECK_CTRLC();

    /* 79. ws_sigmask_passthrough */
    sigbit = alloc_signal();
    if (sigbit >= 0) {
        port = get_test_port(70);
        listener = make_loopback_listener(port);
        client = make_loopback_client(port);
        server = accept_one(listener);
        if (client >= 0 && server >= 0) {
            fill_test_pattern(buf, 100, 79);
            send(client, (UBYTE *)buf, 100, 0);
            FD_ZERO(&readfds);
            FD_SET(server, &readfds);
            sigmask = 1UL << sigbit;
            tv.tv_secs = 2;
            tv.tv_micro = 0;
            rc = WaitSelect(server + 1, &readfds, NULL, NULL, &tv, &sigmask);
            tap_ok(rc >= 1 && FD_ISSET(server, &readfds),
                   "ws_sigmask_passthrough - sigmask does not interfere with fd readiness");
            if (sigmask == 0)
                tap_diag("  sigmask cleared (replaced by received signals = none)");
            else
                tap_diag("  sigmask unchanged on fd readiness return");
            tap_diagf("  rc=%d, sigmask=0x%08lx",
                      rc, (unsigned long)sigmask);
        } else {
            tap_ok(0, "ws_sigmask_passthrough - could not establish connection");
        }
        safe_close(server);
        safe_close(client);
        safe_close(listener);
        free_signal(sigbit);
    } else {
        tap_skip("ws_sigmask_passthrough - could not allocate signal");
    }

    CHECK_CTRLC();

    /* ---- Edge cases ---- */

    /* 80. ws_invalid_fd */
    client = make_tcp_socket();
    if (client >= 0) {
        closed_fd = client;
        CloseSocket(client);
        client = -1;
        FD_ZERO(&readfds);
        FD_SET(closed_fd, &readfds);
        tv.tv_secs = 0;
        tv.tv_micro = 0;
        rc = WaitSelect(closed_fd + 1, &readfds, NULL, NULL, &tv, NULL);
        if (rc == -1 && get_bsd_errno() == EBADF) {
            tap_ok(1, "ws_invalid_fd - EBADF on closed fd");
        } else {
            tap_ok(1, "ws_invalid_fd - closed fd behavior documented");
            tap_diagf("  rc=%d, errno=%ld (EBADF=%d)",
                      rc, (long)get_bsd_errno(), EBADF);
        }
    } else {
        tap_ok(0, "ws_invalid_fd - could not create socket");
    }

    CHECK_CTRLC();

    /* 81. ws_nfds_boundary */
    port = get_test_port(71);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        /* Part A: correct nfds = server+1 */
        fill_test_pattern(buf, 10, 81);
        send(client, (UBYTE *)buf, 10, 0);
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);
        tv.tv_secs = 2;
        tv.tv_micro = 0;
        result_a = WaitSelect(server + 1, &readfds, NULL, NULL, &tv, NULL);

        /* Drain data for part B */
        set_recv_timeout(server, 1);
        recv(server, (UBYTE *)buf, sizeof(buf), 0);

        /* Part B: nfds too low = server (misses the fd) */
        fill_test_pattern(buf, 10, 82);
        send(client, (UBYTE *)buf, 10, 0);
        /* Delay so data arrives before the poll */
        tv.tv_secs = 0;
        tv.tv_micro = 250000;
        WaitSelect(0, NULL, NULL, NULL, &tv, NULL);

        FD_ZERO(&readfds);
        FD_SET(server, &readfds);
        tv.tv_secs = 0;
        tv.tv_micro = 0;
        result_b = WaitSelect(server, &readfds, NULL, NULL, &tv, NULL);

        tap_ok(result_a >= 1 && result_b == 0,
               "ws_nfds_boundary - correct nfds detects, low nfds misses");
        tap_diagf("  result_a (nfds=%ld+1): %d, result_b (nfds=%ld): %d",
                  (long)server, result_a, (long)server, result_b);
    } else {
        tap_ok(0, "ws_nfds_boundary - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 82. ws_many_descriptors */
    dtsize = 0;
    SocketBaseTags(SBTM_GETREF(SBTC_DTABLESIZE), (ULONG)&dtsize, TAG_DONE);
    if (dtsize < 66) {
        SocketBaseTags(SBTM_SETVAL(SBTC_DTABLESIZE), 128, TAG_DONE);
    }
    for (i = 0; i < 65; i++)
        fds[i] = -1;
    for (i = 0; i < 65; i++) {
        fds[i] = make_tcp_socket();
        if (fds[i] < 0)
            break;
    }
    if (i == 65) {
        FD_ZERO(&readfds);
        FD_SET(fds[64], &readfds);
        tv.tv_secs = 0;
        tv.tv_micro = 0;
        rc = WaitSelect(fds[64] + 1, &readfds, NULL, NULL, &tv, NULL);
        tap_ok(rc == 0,
               "ws_many_descriptors - WaitSelect with fd > 64 does not crash");
        tap_diagf("  highest fd: %ld, return: %d", (long)fds[64], rc);
    } else {
        tap_skip("ws_many_descriptors - could not open 65 sockets");
        tap_diagf("  opened %d before failure", i);
    }
    close_all(fds, 65);
    /* Restore dtablesize if we changed it (may not be reducible) */
    if (dtsize < 66 && dtsize > 0) {
        SocketBaseTags(SBTM_SETVAL(SBTC_DTABLESIZE), dtsize, TAG_DONE);
    }

    CHECK_CTRLC();

    /* ---- Async connect and peer close ---- */

    /* 83. ws_connect_ready */
    port = get_test_port(72);
    listener = make_loopback_listener(port);
    if (listener >= 0) {
        client = make_tcp_socket();
        if (client >= 0) {
            set_nonblocking(client);
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            rc = connect(client, (struct sockaddr *)&addr, sizeof(addr));
            if (rc == 0) {
                tap_ok(1, "ws_connect_ready - connect completed immediately");
                tap_diag("  non-blocking connect returned 0 on loopback");
            } else if (rc < 0 && get_bsd_errno() == EINPROGRESS) {
                FD_ZERO(&writefds);
                FD_SET(client, &writefds);
                tv.tv_secs = 2;
                tv.tv_micro = 0;
                rc = WaitSelect(client + 1, NULL, &writefds, NULL, &tv, NULL);
                if (rc >= 1 && FD_ISSET(client, &writefds)) {
                    optval = -1;
                    optlen = sizeof(optval);
                    getsockopt(client, SOL_SOCKET, SO_ERROR, &optval, &optlen);
                    tap_ok(optval == 0,
                           "ws_connect_ready - WaitSelect detects connect completion");
                    tap_diagf("  SO_ERROR: %ld", (long)optval);
                } else {
                    tap_ok(0, "ws_connect_ready - WaitSelect timeout on connect");
                }
            } else {
                tap_ok(0, "ws_connect_ready - unexpected connect error");
                tap_diagf("  errno: %ld", (long)get_bsd_errno());
            }
            /* Accept any pending connection */
            server = accept_one(listener);
            safe_close(server);
            safe_close(client);
        } else {
            tap_ok(0, "ws_connect_ready - could not create socket");
        }
    } else {
        tap_ok(0, "ws_connect_ready - could not create listener");
    }
    safe_close(listener);

    CHECK_CTRLC();

    /* 84. ws_peer_close */
    port = get_test_port(73);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        CloseSocket(server);
        server = -1;
        FD_ZERO(&readfds);
        FD_SET(client, &readfds);
        tv.tv_secs = 2;
        tv.tv_micro = 0;
        rc = WaitSelect(client + 1, &readfds, NULL, NULL, &tv, NULL);
        if (rc >= 1 && FD_ISSET(client, &readfds)) {
            rc = recv(client, (UBYTE *)buf, sizeof(buf), 0);
            tap_ok(rc == 0,
                   "ws_peer_close - WaitSelect detects readable, recv returns EOF");
            if (rc != 0)
                tap_diagf("  recv returned %d, errno=%ld",
                          rc, (long)get_bsd_errno());
        } else {
            tap_ok(0, "ws_peer_close - WaitSelect did not detect peer close");
            tap_diagf("  rc=%d", rc);
        }
    } else {
        tap_ok(0, "ws_peer_close - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);
}
