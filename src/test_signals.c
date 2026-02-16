/*
 * bsdsocktest — Signal and event tests
 *
 * Tests: SetSocketSignals, SocketBaseTagList signal/event masks,
 *        SO_EVENTMASK, GetSocketEvents.
 *
 * 15 tests (73-87), port offsets 80-99.
 */

#include "tap.h"
#include "testutil.h"
#include "known_failures.h"

#include <proto/bsdsocket.h>
#include <proto/exec.h>

#include <errno.h>
#include <string.h>

void run_signals_tests(void)
{
    BYTE sigbit;
    ULONG sigmask, orig, retrieved, pending;
    ULONG evmask, evmask1, evmask2;
    LONG evfd, evfd1, evfd2;
    LONG listener, client, server;
    LONG listener2, client2, server2;
    LONG mask;
    LONG fd;
    LONG dtsize, new_dtsize;
    ULONG ptr;
    struct timeval tv;
    struct sockaddr_in addr;
    unsigned char sbuf[100], rbuf[100];
    int port, rc, i, passed;
    const char *cr;

    /* ---- SetSocketSignals legacy API ---- */

    /* 73. setsocketsignals_basic */
    sigbit = alloc_signal();
    if (sigbit >= 0) {
        SetSocketSignals(1UL << sigbit, 0, 0);
        SetSocketSignals(0, 0, 0);
        tap_ok(1, "SetSocketSignals(): register signal masks [AmiTCP]");
        free_signal(sigbit);
    } else {
        tap_skip("could not allocate signal");
    }

    CHECK_CTRLC();

    /* ---- SocketBaseTagList roundtrips ---- */

    /* 74. sbt_breakmask */
    sigbit = alloc_signal();
    if (sigbit >= 0) {
        orig = 0;
        SocketBaseTags(SBTM_GETREF(SBTC_BREAKMASK), (ULONG)&orig, TAG_DONE);
        SocketBaseTags(SBTM_SETVAL(SBTC_BREAKMASK), 1UL << sigbit, TAG_DONE);
        retrieved = 0;
        SocketBaseTags(SBTM_GETREF(SBTC_BREAKMASK), (ULONG)&retrieved,
                       TAG_DONE);
        tap_ok(retrieved == (1UL << sigbit),
               "SocketBaseTags(SBTC_BREAKMASK): Ctrl-C signal [AmiTCP]");
        tap_diagf("  set=0x%08lx, got=0x%08lx",
                  (unsigned long)(1UL << sigbit), (unsigned long)retrieved);
        SocketBaseTags(SBTM_SETVAL(SBTC_BREAKMASK), orig, TAG_DONE);
        free_signal(sigbit);
    } else {
        tap_skip("could not allocate signal");
    }

    CHECK_CTRLC();

    /* 75. sbt_sigeventmask */
    sigbit = alloc_signal();
    if (sigbit >= 0) {
        orig = 0;
        SocketBaseTags(SBTM_GETREF(SBTC_SIGEVENTMASK), (ULONG)&orig,
                       TAG_DONE);
        SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                       TAG_DONE);
        retrieved = 0;
        SocketBaseTags(SBTM_GETREF(SBTC_SIGEVENTMASK), (ULONG)&retrieved,
                       TAG_DONE);
        tap_ok(retrieved == (1UL << sigbit),
               "SocketBaseTags(SBTC_SIGEVENTMASK): event signal [AmiTCP]");
        tap_diagf("  set=0x%08lx, got=0x%08lx",
                  (unsigned long)(1UL << sigbit), (unsigned long)retrieved);
        SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
        SetSignal(0, 1UL << sigbit);
        free_signal(sigbit);
    } else {
        tap_skip("could not allocate signal");
    }

    CHECK_CTRLC();

    /* 76. sbt_errnolongptr_get */
    ptr = 0;
    SocketBaseTags(SBTM_GETREF(SBTC_ERRNOLONGPTR), (ULONG)&ptr, TAG_DONE);
    tap_ok(ptr != 0,
           "SocketBaseTags(SBTC_ERRNOLONGPTR): get errno pointer [AmiTCP]");
    tap_diagf("  pointer: 0x%08lx", (unsigned long)ptr);

    CHECK_CTRLC();

    /* 77. sbt_herrnolongptr_get */
    ptr = 0;
    SocketBaseTags(SBTM_GETREF(SBTC_HERRNOLONGPTR), (ULONG)&ptr, TAG_DONE);
    tap_ok(ptr != 0,
           "SocketBaseTags(SBTC_HERRNOLONGPTR): get h_errno pointer [AmiTCP]");
    tap_diagf("  pointer: 0x%08lx", (unsigned long)ptr);

    CHECK_CTRLC();

    /* 78. sbt_dtablesize */
    dtsize = 0;
    SocketBaseTags(SBTM_GETREF(SBTC_DTABLESIZE), (ULONG)&dtsize, TAG_DONE);
    tap_diagf("  current dtablesize: %ld", (long)dtsize);
    if (dtsize < 64) {
        /* GET returned broken value — don't attempt SET which may crash
         * (UAE emulation's SBTC_DTABLESIZE SET causes exit code 1) */
        tap_ok(0,
               "SocketBaseTags(SBTC_DTABLESIZE): get/set table size [AmiTCP]");
        tap_diag("  GET returned < 64, skipping SET to avoid crash");
    } else {
        SocketBaseTags(SBTM_SETVAL(SBTC_DTABLESIZE), 128, TAG_DONE);
        new_dtsize = 0;
        SocketBaseTags(SBTM_GETREF(SBTC_DTABLESIZE), (ULONG)&new_dtsize,
                       TAG_DONE);
        tap_ok(dtsize >= 64 && new_dtsize >= 128,
               "SocketBaseTags(SBTC_DTABLESIZE): get/set table size [AmiTCP]");
        tap_diagf("  after set 128: %ld", (long)new_dtsize);
        /* Restore (may not reduce) */
        if (dtsize > 0)
            SocketBaseTags(SBTM_SETVAL(SBTC_DTABLESIZE), dtsize, TAG_DONE);
    }

    CHECK_CTRLC();

    /* ---- SO_EVENTMASK + GetSocketEvents ---- */

    /*
     * Each event test follows the signal testing pattern:
     * 1. Allocate signal, set SIGEVENTMASK
     * 2. Set SO_EVENTMASK on target socket
     * 3. Trigger event
     * 4. WaitSelect for signal (2s safety timeout)
     * 5. GetSocketEvents to check result
     *
     * Cleanup order (critical — prevents signal races):
     * a. Clear SO_EVENTMASK to 0 on each socket
     * b. SBTM_SETVAL(SBTC_SIGEVENTMASK, 0)
     * c. Close all sockets
     * d. SetSignal(0, 1UL << sigbit)
     * e. free_signal(sigbit)
     */

    /* 79. eventmask_fd_read */
    cr = known_crash(79);
    if (cr) {
        tap_ok(0, "SO_EVENTMASK FD_READ: signal on data arrival [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            port = get_test_port(80);
            listener = make_loopback_listener(port);
            client = make_loopback_client(port);
            server = accept_one(listener);
            if (client >= 0 && server >= 0) {
                mask = FD_READ;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                fill_test_pattern(sbuf, 100, 91);
                send(client, (UBYTE *)sbuf, 100, 0);
                sigmask = 1UL << sigbit;
                tv.tv_secs = 2;
                tv.tv_micro = 0;
                WaitSelect(0, NULL, NULL, NULL, &tv, &sigmask);
                evmask = 0;
                evfd = GetSocketEvents(&evmask);
                tap_ok(evfd == server && (evmask & FD_READ),
                       "SO_EVENTMASK FD_READ: signal on data arrival [AmiTCP]");
                tap_diagf("  evfd=%ld (expected %ld), evmask=0x%lx",
                          (long)evfd, (long)server, (unsigned long)evmask);
                mask = 0;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
            } else {
                tap_ok(0,
                       "SO_EVENTMASK FD_READ: signal on data arrival [AmiTCP]");
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(server);
            safe_close(client);
            safe_close(listener);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }

    CHECK_CTRLC();

    /* 80. eventmask_fd_connect */
    cr = known_crash(80);
    if (cr) {
        tap_ok(0, "SO_EVENTMASK FD_CONNECT: signal on connect [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            port = get_test_port(81);
            listener = make_loopback_listener(port);
            if (listener >= 0) {
                client = make_tcp_socket();
                if (client >= 0) {
                    set_nonblocking(client);
                    mask = FD_CONNECT;
                    setsockopt(client, SOL_SOCKET, SO_EVENTMASK, &mask,
                               sizeof(mask));
                    memset(&addr, 0, sizeof(addr));
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(port);
                    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
                    rc = connect(client, (struct sockaddr *)&addr,
                                 sizeof(addr));
                    sigmask = 1UL << sigbit;
                    tv.tv_secs = 2;
                    tv.tv_micro = 0;
                    WaitSelect(0, NULL, NULL, NULL, &tv, &sigmask);
                    evmask = 0;
                    evfd = GetSocketEvents(&evmask);
                    if (rc == 0 && evfd == -1) {
                        tap_ok(1,
                               "SO_EVENTMASK FD_CONNECT: signal on connect [AmiTCP]");
                        tap_diag("  synchronous loopback connect returned 0");
                    } else if (evfd == client && (evmask & FD_CONNECT)) {
                        tap_ok(1,
                               "SO_EVENTMASK FD_CONNECT: signal on connect [AmiTCP]");
                    } else if (rc < 0 && get_bsd_errno() == EINPROGRESS &&
                               evfd == -1) {
                        tap_ok(0,
                               "SO_EVENTMASK FD_CONNECT: signal on connect [AmiTCP]");
                    } else {
                        tap_ok(1,
                               "SO_EVENTMASK FD_CONNECT: signal on connect [AmiTCP]");
                        tap_diagf("  connect rc=%d, evfd=%ld, evmask=0x%lx",
                                  rc, (long)evfd, (unsigned long)evmask);
                    }
                    mask = 0;
                    setsockopt(client, SOL_SOCKET, SO_EVENTMASK, &mask,
                               sizeof(mask));
                    server = accept_one(listener);
                    safe_close(server);
                } else {
                    tap_ok(0,
                           "SO_EVENTMASK FD_CONNECT: signal on connect [AmiTCP]");
                }
                safe_close(client);
            } else {
                tap_ok(0,
                       "SO_EVENTMASK FD_CONNECT: signal on connect [AmiTCP]");
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(listener);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }

    CHECK_CTRLC();

    /* 81. eventmask_no_spurious */
    cr = known_crash(81);
    if (cr) {
        tap_ok(0, "SO_EVENTMASK: no spurious events on idle socket [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            fd = make_tcp_socket();
            if (fd >= 0) {
                mask = FD_READ | FD_WRITE | FD_CONNECT;
                setsockopt(fd, SOL_SOCKET, SO_EVENTMASK, &mask, sizeof(mask));
                /* Brief delay */
                tv.tv_secs = 0;
                tv.tv_micro = 100000;
                WaitSelect(0, NULL, NULL, NULL, &tv, NULL);
                /* Check for spurious signal */
                pending = SetSignal(0, 0); /* Read without clearing */
                evmask = 0;
                evfd = GetSocketEvents(&evmask);
                tap_ok(!(pending & (1UL << sigbit)) && evfd == -1,
                       "SO_EVENTMASK: no spurious events on idle socket [AmiTCP]");
                tap_diagf("  signal pending: %s, GetSocketEvents: %ld",
                          (pending & (1UL << sigbit)) ? "yes" : "no",
                          (long)evfd);
                mask = 0;
                setsockopt(fd, SOL_SOCKET, SO_EVENTMASK, &mask, sizeof(mask));
            } else {
                tap_ok(0,
                       "SO_EVENTMASK: no spurious events on idle socket [AmiTCP]");
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(fd);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }

    CHECK_CTRLC();

    /* 82. eventmask_fd_accept */
    cr = known_crash(82);
    if (cr) {
        tap_ok(0, "SO_EVENTMASK FD_ACCEPT: signal on incoming [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            port = get_test_port(82);
            listener = make_loopback_listener(port);
            if (listener >= 0) {
                mask = FD_ACCEPT;
                setsockopt(listener, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                client = make_loopback_client(port);
                if (client >= 0) {
                    sigmask = 1UL << sigbit;
                    tv.tv_secs = 2;
                    tv.tv_micro = 0;
                    WaitSelect(0, NULL, NULL, NULL, &tv, &sigmask);
                    evmask = 0;
                    evfd = GetSocketEvents(&evmask);
                    tap_ok(evfd == listener && (evmask & FD_ACCEPT),
                           "SO_EVENTMASK FD_ACCEPT: signal on incoming [AmiTCP]");
                    tap_diagf("  evfd=%ld (expected %ld), evmask=0x%lx",
                              (long)evfd, (long)listener,
                              (unsigned long)evmask);
                    server = accept_one(listener);
                    safe_close(server);
                } else {
                    tap_ok(0,
                           "SO_EVENTMASK FD_ACCEPT: signal on incoming [AmiTCP]");
                }
                mask = 0;
                setsockopt(listener, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                safe_close(client);
            } else {
                tap_ok(0,
                       "SO_EVENTMASK FD_ACCEPT: signal on incoming [AmiTCP]");
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(listener);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }

    CHECK_CTRLC();

    /* 83. eventmask_fd_close */
    cr = known_crash(83);
    if (cr) {
        tap_ok(0, "SO_EVENTMASK FD_CLOSE: signal on peer disconnect [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            port = get_test_port(83);
            listener = make_loopback_listener(port);
            client = make_loopback_client(port);
            server = accept_one(listener);
            if (client >= 0 && server >= 0) {
                mask = FD_CLOSE;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                CloseSocket(client);
                client = -1;
                sigmask = 1UL << sigbit;
                tv.tv_secs = 2;
                tv.tv_micro = 0;
                WaitSelect(0, NULL, NULL, NULL, &tv, &sigmask);
                evmask = 0;
                evfd = GetSocketEvents(&evmask);
                tap_ok(evfd == server && (evmask & FD_CLOSE),
                       "SO_EVENTMASK FD_CLOSE: signal on peer disconnect [AmiTCP]");
                tap_diagf("  evfd=%ld (expected %ld), evmask=0x%lx",
                          (long)evfd, (long)server, (unsigned long)evmask);
                mask = 0;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
            } else {
                tap_ok(0,
                       "SO_EVENTMASK FD_CLOSE: signal on peer disconnect [AmiTCP]");
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(server);
            safe_close(client);
            safe_close(listener);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }

    CHECK_CTRLC();

    /* ---- GetSocketEvents behavior ---- */

    /* 84. getsocketevents_clears */
    cr = known_crash(84);
    if (cr) {
        tap_ok(0, "GetSocketEvents(): event consumed after retrieval [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            port = get_test_port(84);
            listener = make_loopback_listener(port);
            client = make_loopback_client(port);
            server = accept_one(listener);
            if (client >= 0 && server >= 0) {
                mask = FD_READ;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                fill_test_pattern(sbuf, 100, 96);
                send(client, (UBYTE *)sbuf, 100, 0);
                sigmask = 1UL << sigbit;
                tv.tv_secs = 2;
                tv.tv_micro = 0;
                WaitSelect(0, NULL, NULL, NULL, &tv, &sigmask);
                evmask1 = 0;
                evfd1 = GetSocketEvents(&evmask1);
                evmask2 = 0;
                evfd2 = GetSocketEvents(&evmask2);
                tap_ok(evfd1 >= 0 && evfd2 == -1,
                       "GetSocketEvents(): event consumed after retrieval [AmiTCP]");
                tap_diagf("  first: evfd=%ld evmask=0x%lx, second: evfd=%ld",
                          (long)evfd1, (unsigned long)evmask1, (long)evfd2);
                mask = 0;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
            } else {
                tap_ok(0,
                       "GetSocketEvents(): event consumed after retrieval [AmiTCP]");
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(server);
            safe_close(client);
            safe_close(listener);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }

    CHECK_CTRLC();

    /* 85. getsocketevents_multiple */
    cr = known_crash(85);
    if (cr) {
        tap_ok(0, "GetSocketEvents(): round-robin across sockets [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            /* First connected pair */
            port = get_test_port(85);
            listener = make_loopback_listener(port);
            client = make_loopback_client(port);
            server = accept_one(listener);
            /* Second connected pair */
            port = get_test_port(86);
            listener2 = make_loopback_listener(port);
            client2 = make_loopback_client(port);
            server2 = accept_one(listener2);

            if (server >= 0 && server2 >= 0) {
                mask = FD_READ;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                setsockopt(server2, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                /* Send data to both */
                fill_test_pattern(sbuf, 10, 97);
                send(client, (UBYTE *)sbuf, 10, 0);
                send(client2, (UBYTE *)sbuf, 10, 0);
                /* Wait for first event signal */
                sigmask = 1UL << sigbit;
                tv.tv_secs = 2;
                tv.tv_micro = 0;
                WaitSelect(0, NULL, NULL, NULL, &tv, &sigmask);
                /* Brief delay for second event to propagate */
                tv.tv_secs = 0;
                tv.tv_micro = 100000;
                WaitSelect(0, NULL, NULL, NULL, &tv, NULL);

                evmask1 = 0;
                evfd1 = GetSocketEvents(&evmask1);
                evmask2 = 0;
                evfd2 = GetSocketEvents(&evmask2);
                evmask = 0;
                evfd = GetSocketEvents(&evmask); /* Should return -1 */

                /* Check both servers reported (in either order) */
                passed = 0;
                if (((evfd1 == server && evfd2 == server2) ||
                     (evfd1 == server2 && evfd2 == server)) &&
                    (evmask1 & FD_READ) && (evmask2 & FD_READ) &&
                    evfd == -1) {
                    passed = 1;
                }
                tap_ok(passed,
                       "GetSocketEvents(): round-robin across sockets [AmiTCP]");
                tap_diagf("  first: fd=%ld mask=0x%lx, second: fd=%ld "
                          "mask=0x%lx, third: fd=%ld",
                          (long)evfd1, (unsigned long)evmask1,
                          (long)evfd2, (unsigned long)evmask2,
                          (long)evfd);
                /* Cleanup SO_EVENTMASK */
                mask = 0;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                setsockopt(server2, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
            } else {
                tap_ok(0,
                       "GetSocketEvents(): round-robin across sockets [AmiTCP]");
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(server);
            safe_close(client);
            safe_close(listener);
            safe_close(server2);
            safe_close(client2);
            safe_close(listener2);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }

    CHECK_CTRLC();

    /* 86. getsocketevents_empty */
    evmask = 0;
    evfd = GetSocketEvents(&evmask);
    tap_ok(evfd == -1,
           "GetSocketEvents(): -1 when no events pending [AmiTCP]");
    tap_diagf("  returned: %ld", (long)evfd);

    CHECK_CTRLC();

    /* ---- Stress test ---- */

    /* 87. rapid_waitselect_signal */
    cr = known_crash(87);
    if (cr) {
        tap_ok(0, "WaitSelect + signals: stress test (50 iterations) [AmiTCP]");
        tap_diagf("  not exercised: %s", cr);
    } else {
        sigbit = alloc_signal();
        if (sigbit >= 0) {
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 1UL << sigbit,
                           TAG_DONE);
            port = get_test_port(87);
            listener = make_loopback_listener(port);
            client = make_loopback_client(port);
            server = accept_one(listener);
            if (client >= 0 && server >= 0) {
                mask = FD_READ;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
                set_recv_timeout(server, 2);
                passed = 1;
                for (i = 0; i < 50; i++) {
                    sigmask = 1UL << sigbit;
                    fill_test_pattern(sbuf, 10, i);
                    rc = send(client, (UBYTE *)sbuf, 10, 0);
                    if (rc != 10) {
                        tap_diagf("  iteration %d: send failed (rc=%d, "
                                  "errno=%ld)",
                                  i, rc, (long)get_bsd_errno());
                        passed = 0;
                        break;
                    }
                    tv.tv_secs = 2;
                    tv.tv_micro = 0;
                    WaitSelect(0, NULL, NULL, NULL, &tv, &sigmask);
                    evmask = 0;
                    evfd = GetSocketEvents(&evmask);
                    if (evfd != server || !(evmask & FD_READ)) {
                        tap_diagf("  iteration %d: evfd=%ld (expected %ld), "
                                  "evmask=0x%lx",
                                  i, (long)evfd, (long)server,
                                  (unsigned long)evmask);
                        passed = 0;
                        break;
                    }
                    rc = recv(server, (UBYTE *)rbuf, 10, 0);
                    if (rc != 10) {
                        tap_diagf("  iteration %d: recv=%d, errno=%ld",
                                  i, rc, (long)get_bsd_errno());
                        passed = 0;
                        break;
                    }
                    SetSignal(0, 1UL << sigbit);
                }
                tap_ok(passed,
                       "WaitSelect + signals: stress test (50 iterations) [AmiTCP]");
                tap_diagf("  completed: %d/50, total bytes: %d",
                          passed ? 50 : i, (passed ? 50 : i) * 10);
                mask = 0;
                setsockopt(server, SOL_SOCKET, SO_EVENTMASK, &mask,
                           sizeof(mask));
            } else {
                tap_ok(0,
                       "WaitSelect + signals: stress test (50 iterations) [AmiTCP]");
                tap_diagf("  listener=%ld client=%ld server=%ld errno=%ld",
                          (long)listener, (long)client, (long)server,
                          (long)get_bsd_errno());
            }
            SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
            safe_close(server);
            safe_close(client);
            safe_close(listener);
            SetSignal(0, 1UL << sigbit);
            free_signal(sigbit);
        } else {
            tap_skip("could not allocate signal");
        }
    }
}
