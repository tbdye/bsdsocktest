/*
 * bsdsocktest — Send/receive tests
 *
 * Tests: send, recv, sendto, recvfrom, sendmsg, recvmsg,
 *        MSG_PEEK, MSG_OOB, non-blocking behavior.
 *
 * 19 tests (24-42), port offsets 20-39 (loopback) and 160-179 (network).
 */

#include "tap.h"
#include "testutil.h"
#include "helper_proto.h"

#include <proto/bsdsocket.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

void run_sendrecv_tests(void)
{
    static unsigned char sbuf[8192], rbuf[8192];
    LONG listener, client, server, fd_a, fd_b, fd_dummy;
    struct sockaddr_in addr, from_addr;
    socklen_t addrlen;
    int port, rc, total, mismatch;
    int passed, attempts;
    LONG one = 1;
    struct msghdr msg;
    struct iovec iov[3];

    /* ---- Basic send/recv ---- */

    /* 24. sendrecv_basic_100 */
    port = get_test_port(20);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        fill_test_pattern(sbuf, 100, 1);
        send(client, (UBYTE *)sbuf, 100, 0);
        set_recv_timeout(server, 2);
        rc = recv(server, (UBYTE *)rbuf, sizeof(rbuf), 0);
        mismatch = verify_test_pattern(rbuf, 100, 1);
        tap_ok(rc == 100 && mismatch == 0,
               "send()/recv(): 100-byte TCP transfer [BSD 4.4]");
    } else {
        tap_ok(0, "send()/recv(): 100-byte TCP transfer [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 25. sendrecv_large_8192 */
    port = get_test_port(21);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        fill_test_pattern(sbuf, 8192, 2);
        send(client, (UBYTE *)sbuf, 8192, 0);
        /* Recv in a loop — TCP may fragment */
        set_recv_timeout(server, 3);
        total = 0;
        while (total < 8192) {
            rc = recv(server, (UBYTE *)rbuf + total, 8192 - total, 0);
            if (rc <= 0)
                break;
            total += rc;
        }
        mismatch = verify_test_pattern(rbuf, 8192, 2);
        tap_ok(total == 8192 && mismatch == 0,
               "send()/recv(): 8192-byte TCP transfer (multi-recv) [BSD 4.4]");
        if (total != 8192)
            tap_diagf("  received %d of 8192 bytes", total);
    } else {
        tap_ok(0, "send()/recv(): 8192-byte TCP transfer (multi-recv) [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- MSG_PEEK ---- */

    /* 26. recv_msg_peek */
    port = get_test_port(22);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        fill_test_pattern(sbuf, 50, 3);
        send(client, (UBYTE *)sbuf, 50, 0);
        /* Peek — should see data without consuming */
        set_recv_timeout(server, 2);
        rc = recv(server, (UBYTE *)rbuf, sizeof(rbuf), MSG_PEEK);
        mismatch = verify_test_pattern(rbuf, 50, 3);
        if (rc == 50 && mismatch == 0) {
            /* Normal recv — should see same data */
            memset(rbuf, 0, sizeof(rbuf));
            rc = recv(server, (UBYTE *)rbuf, sizeof(rbuf), 0);
            mismatch = verify_test_pattern(rbuf, 50, 3);
            tap_ok(rc == 50 && mismatch == 0,
                   "recv(MSG_PEEK): read without consuming [BSD 4.4]");
        } else {
            tap_ok(0, "recv(MSG_PEEK): read without consuming [BSD 4.4]");
        }
    } else {
        tap_ok(0, "recv(MSG_PEEK): read without consuming [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- MSG_OOB ---- */

    /* 27. sendrecv_msg_oob */
    port = get_test_port(23);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        sbuf[0] = 0xAB;
        rc = send(client, (UBYTE *)sbuf, 1, MSG_OOB);
        if (rc < 0) {
            tap_skip("MSG_OOB send not supported");
        } else {
            set_recv_timeout(server, 2);
            rbuf[0] = 0;
            rc = recv(server, (UBYTE *)rbuf, 1, MSG_OOB);
            if (rc == 1 && rbuf[0] == 0xAB) {
                tap_ok(1, "recv(MSG_OOB): urgent data delivery [BSD 4.4]");
            } else {
                tap_ok(0, "recv(MSG_OOB): urgent data delivery [BSD 4.4]");
                tap_diagf("  recv(MSG_OOB): rc=%d byte=0x%02x errno=%ld",
                          rc, (int)(unsigned char)rbuf[0],
                          (long)get_bsd_errno());
            }
        }
    } else {
        tap_ok(0, "recv(MSG_OOB): urgent data delivery [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- UDP sendto/recvfrom ---- */

    /* 28. udp_sendto_recvfrom */
    port = get_test_port(24);
    fd_a = make_udp_socket();
    fd_b = make_udp_socket();
    if (fd_a >= 0 && fd_b >= 0) {
        one = 1;
        setsockopt(fd_b, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(fd_b, (struct sockaddr *)&addr, sizeof(addr));

        fill_test_pattern(sbuf, 100, 3);
        sendto(fd_a, (UBYTE *)sbuf, 100, 0,
               (struct sockaddr *)&addr, sizeof(addr));

        set_recv_timeout(fd_b, 2);
        addrlen = sizeof(from_addr);
        rc = recvfrom(fd_b, (UBYTE *)rbuf, sizeof(rbuf), 0,
                      (struct sockaddr *)&from_addr, &addrlen);
        mismatch = verify_test_pattern(rbuf, 100, 3);
        tap_ok(rc == 100 && mismatch == 0 &&
               from_addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
               "sendto()/recvfrom(): UDP datagram loopback [RFC 768]");
    } else {
        tap_ok(0, "sendto()/recvfrom(): UDP datagram loopback [RFC 768]");
    }
    safe_close(fd_a);
    safe_close(fd_b);

    CHECK_CTRLC();

    /* 29. udp_sendto_after_prior_ops — exercises fd allocator
     * to catch Amiberry Bug #1 (sendto checks stale sb->s). */
    port = get_test_port(25);
    fd_dummy = make_tcp_socket();
    safe_close(fd_dummy); /* Exercise fd allocator */
    fd_a = make_udp_socket();
    fd_b = make_udp_socket();
    if (fd_a >= 0 && fd_b >= 0) {
        one = 1;
        setsockopt(fd_b, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(fd_b, (struct sockaddr *)&addr, sizeof(addr));

        fill_test_pattern(sbuf, 100, 4);
        sendto(fd_a, (UBYTE *)sbuf, 100, 0,
               (struct sockaddr *)&addr, sizeof(addr));

        set_recv_timeout(fd_b, 2);
        addrlen = sizeof(from_addr);
        rc = recvfrom(fd_b, (UBYTE *)rbuf, sizeof(rbuf), 0,
                      (struct sockaddr *)&from_addr, &addrlen);
        mismatch = verify_test_pattern(rbuf, 100, 4);
        tap_ok(rc == 100 && mismatch == 0,
               "sendto(): correct dispatch after prior socket ops [BSD 4.4]");
    } else {
        tap_ok(0, "sendto(): correct dispatch after prior socket ops [BSD 4.4]");
    }
    safe_close(fd_a);
    safe_close(fd_b);

    CHECK_CTRLC();

    /* 30. udp_sendto_basic_second */
    port = get_test_port(26);
    fd_a = make_udp_socket();
    fd_b = make_udp_socket();
    if (fd_a >= 0 && fd_b >= 0) {
        one = 1;
        setsockopt(fd_b, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(fd_b, (struct sockaddr *)&addr, sizeof(addr));

        fill_test_pattern(sbuf, 100, 5);
        sendto(fd_a, (UBYTE *)sbuf, 100, 0,
               (struct sockaddr *)&addr, sizeof(addr));

        set_recv_timeout(fd_b, 2);
        addrlen = sizeof(from_addr);
        rc = recvfrom(fd_b, (UBYTE *)rbuf, sizeof(rbuf), 0,
                      (struct sockaddr *)&from_addr, &addrlen);
        mismatch = verify_test_pattern(rbuf, 100, 5);
        tap_ok(rc == 100 && mismatch == 0,
               "sendto(): on independently created socket [BSD 4.4]");
    } else {
        tap_ok(0, "sendto(): on independently created socket [BSD 4.4]");
    }
    safe_close(fd_a);
    safe_close(fd_b);

    CHECK_CTRLC();

    /* ---- sendmsg/recvmsg ---- */

    /* 31. sendmsg_recvmsg_single */
    port = get_test_port(27);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        fill_test_pattern(sbuf, 100, 6);

        memset(&msg, 0, sizeof(msg));
        iov[0].iov_base = sbuf;
        iov[0].iov_len = 100;
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        rc = sendmsg(client, &msg, 0);
        if (rc == 100) {
            memset(&msg, 0, sizeof(msg));
            iov[0].iov_base = rbuf;
            iov[0].iov_len = sizeof(rbuf);
            msg.msg_iov = iov;
            msg.msg_iovlen = 1;
            set_recv_timeout(server, 2);
            rc = recvmsg(server, &msg, 0);
            mismatch = verify_test_pattern(rbuf, 100, 6);
            tap_ok(rc == 100 && mismatch == 0,
                   "sendmsg()/recvmsg(): single iovec [BSD 4.4]");
        } else {
            tap_ok(0, "sendmsg()/recvmsg(): single iovec [BSD 4.4]");
        }
    } else {
        tap_ok(0, "sendmsg()/recvmsg(): single iovec [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 32. sendmsg_recvmsg_scatter */
    port = get_test_port(28);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        fill_test_pattern(sbuf, 100, 7);

        /* Send with 3 iovecs: 50+30+20 = 100 bytes */
        memset(&msg, 0, sizeof(msg));
        iov[0].iov_base = sbuf;
        iov[0].iov_len = 50;
        iov[1].iov_base = sbuf + 50;
        iov[1].iov_len = 30;
        iov[2].iov_base = sbuf + 80;
        iov[2].iov_len = 20;
        msg.msg_iov = iov;
        msg.msg_iovlen = 3;

        rc = sendmsg(client, &msg, 0);
        if (rc == 100) {
            /* Recv with 3 iovecs of same sizes */
            memset(rbuf, 0, sizeof(rbuf));
            memset(&msg, 0, sizeof(msg));
            iov[0].iov_base = rbuf;
            iov[0].iov_len = 50;
            iov[1].iov_base = rbuf + 50;
            iov[1].iov_len = 30;
            iov[2].iov_base = rbuf + 80;
            iov[2].iov_len = 20;
            msg.msg_iov = iov;
            msg.msg_iovlen = 3;

            set_recv_timeout(server, 2);
            rc = recvmsg(server, &msg, 0);
            mismatch = verify_test_pattern(rbuf, 100, 7);
            tap_ok(rc == 100 && mismatch == 0,
                   "sendmsg()/recvmsg(): scatter-gather (multiple iovecs) [BSD 4.4]");
        } else {
            tap_ok(0, "sendmsg()/recvmsg(): scatter-gather (multiple iovecs) [BSD 4.4]");
            tap_diagf("  sendmsg returned %d", rc);
        }
    } else {
        tap_ok(0, "sendmsg()/recvmsg(): scatter-gather (multiple iovecs) [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Non-blocking behavior ---- */

    /* 33. recv_nonblocking_ewouldblock */
    port = get_test_port(29);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (server >= 0) {
        set_nonblocking(server);
        rc = recv(server, (UBYTE *)rbuf, sizeof(rbuf), 0);
        tap_ok(rc < 0 && (get_bsd_errno() == EWOULDBLOCK ||
                           get_bsd_errno() == EAGAIN),
               "recv(): EWOULDBLOCK on empty non-blocking socket [BSD 4.4]");
    } else {
        tap_ok(0, "recv(): EWOULDBLOCK on empty non-blocking socket [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 34. send_nonblocking_ewouldblock */
    port = get_test_port(30);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        set_nonblocking(client);
        fill_test_pattern(sbuf, sizeof(sbuf), 8);
        total = 0;
        /* Send in a loop until buffer full */
        while (total < 1048576) { /* Cap at 1MB */
            rc = send(client, (UBYTE *)sbuf, sizeof(sbuf), 0);
            if (rc < 0)
                break;
            total += rc;
        }
        if (rc < 0 && (get_bsd_errno() == EWOULDBLOCK ||
                        get_bsd_errno() == EAGAIN)) {
            tap_ok(1, "send(): EWOULDBLOCK when buffer full [BSD 4.4]");
            tap_diagf("  sent %d bytes before EWOULDBLOCK", total);
        } else {
            tap_skip("send buffer never filled (>1MB)");
        }
    } else {
        tap_ok(0, "send(): EWOULDBLOCK when buffer full [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Send after peer close ---- */

    /* 35. send_after_peer_close */
    port = get_test_port(31);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        safe_close(server);
        server = -1;
        /* Drain the FIN notification */
        set_recv_timeout(client, 1);
        recv(client, (UBYTE *)rbuf, sizeof(rbuf), 0);
        /* On loopback, the peer socket is fully gone after CloseSocket(),
         * so there is no endpoint left to generate a RST.  On a real
         * network the remote kernel would RST, but on loopback the data
         * may simply be discarded.  Try multiple sends to give the stack
         * every chance; if it never errors, the known-failures system
         * handles the expected failure for stacks that behave this way. */
        fill_test_pattern(sbuf, 100, 9);
        passed = 0;
        for (attempts = 0; attempts < 5; attempts++) {
            rc = send(client, (UBYTE *)sbuf, 100, 0);
            if (rc < 0) {
                passed = (get_bsd_errno() == EPIPE ||
                          get_bsd_errno() == ECONNRESET);
                break;
            }
            /* Let RST arrive */
            set_recv_timeout(client, 1);
            rc = recv(client, (UBYTE *)rbuf, 1, 0);
            if (rc < 0 && (get_bsd_errno() == ECONNRESET ||
                            get_bsd_errno() == EPIPE)) {
                passed = 1;
                break;
            }
        }
        if (passed) {
            tap_ok(1, "send(): error after peer closes connection [BSD 4.4]");
            tap_diagf("  errno: %ld (after %d attempt(s))",
                      (long)get_bsd_errno(), attempts + 1);
        } else {
            tap_ok(0, "send(): error after peer closes connection [BSD 4.4]");
            tap_diagf("  %d attempts without error, last errno: %ld",
                      attempts, (long)get_bsd_errno());
        }
    } else {
        tap_ok(0, "send(): error after peer closes connection [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Bidirectional transfer ---- */

    /* 36. sendrecv_bidirectional */
    port = get_test_port(32);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        /* client -> server (seed 10) */
        fill_test_pattern(sbuf, 200, 10);
        rc = send(client, (UBYTE *)sbuf, 200, 0);
        if (rc != 200) {
            tap_ok(0, "send()/recv(): simultaneous bidirectional transfer [BSD 4.4]");
            tap_diagf("  send(client): rc=%d errno=%ld",
                      rc, (long)get_bsd_errno());
        } else {
            /* server -> client (seed 11) */
            fill_test_pattern(sbuf, 200, 11);
            rc = send(server, (UBYTE *)sbuf, 200, 0);
            if (rc != 200) {
                tap_ok(0, "send()/recv(): simultaneous bidirectional transfer [BSD 4.4]");
                tap_diagf("  send(server): rc=%d errno=%ld",
                          rc, (long)get_bsd_errno());
            } else {
                /* Receive both sides (loop for short reads) */
                set_recv_timeout(server, 2);
                total = 0;
                while (total < 200) {
                    rc = recv(server, (UBYTE *)rbuf + total, 200 - total, 0);
                    if (rc <= 0)
                        break;
                    total += rc;
                }
                mismatch = verify_test_pattern(rbuf, 200, 10);
                if (total == 200 && mismatch == 0) {
                    set_recv_timeout(client, 2);
                    total = 0;
                    while (total < 200) {
                        rc = recv(client, (UBYTE *)rbuf + total,
                                  200 - total, 0);
                        if (rc <= 0)
                            break;
                        total += rc;
                    }
                    mismatch = verify_test_pattern(rbuf, 200, 11);
                    tap_ok(total == 200 && mismatch == 0,
                           "send()/recv(): simultaneous bidirectional transfer [BSD 4.4]");
                    if (total != 200 || mismatch != 0)
                        tap_diagf("  client recv: total=%d mismatch=%d errno=%ld",
                                  total, mismatch, (long)get_bsd_errno());
                } else {
                    tap_ok(0, "send()/recv(): simultaneous bidirectional transfer [BSD 4.4]");
                    tap_diagf("  server recv: total=%d mismatch=%d errno=%ld",
                              total, mismatch, (long)get_bsd_errno());
                }
            }
        }
    } else {
        tap_ok(0, "send()/recv(): simultaneous bidirectional transfer [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- Edge cases ---- */

    /* 37. recv_zero_length */
    port = get_test_port(33);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        fill_test_pattern(sbuf, 10, 12);
        send(client, (UBYTE *)sbuf, 10, 0);
        /* Zero-length recv */
        rc = recv(server, (UBYTE *)rbuf, 0, 0);
        tap_diagf("  recv(len=0) returned %d", rc);
        /* Now recv normally — all 10 bytes should still be there */
        set_recv_timeout(server, 2);
        rc = recv(server, (UBYTE *)rbuf, sizeof(rbuf), 0);
        mismatch = verify_test_pattern(rbuf, 10, 12);
        tap_ok(rc == 10 && mismatch == 0,
               "recv(): behavior with zero-length buffer [BSD 4.4]");
    } else {
        tap_ok(0, "recv(): behavior with zero-length buffer [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 38. send_zero_bytes */
    port = get_test_port(34);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        rc = send(client, (UBYTE *)sbuf, 0, 0);
        tap_diagf("  send(len=0) returned %d", rc);
        /* Verify connection still works */
        fill_test_pattern(sbuf, 10, 13);
        send(client, (UBYTE *)sbuf, 10, 0);
        set_recv_timeout(server, 2);
        rc = recv(server, (UBYTE *)rbuf, sizeof(rbuf), 0);
        mismatch = verify_test_pattern(rbuf, 10, 13);
        tap_ok(rc == 10 && mismatch == 0,
               "send(): zero-length send [BSD 4.4]");
    } else {
        tap_ok(0, "send(): zero-length send [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ==== Network send/recv tests — require host helper ==== */

    if (!helper_is_connected()) {
        tap_skip("host helper not connected");
        CHECK_CTRLC();
        tap_skip("host helper not connected");
        CHECK_CTRLC();
        tap_skip("host helper not connected");
        CHECK_CTRLC();
        tap_skip("host helper not connected");
        return;
    }

    /* 39. tcp_network_64k */
    {
        LONG fd;
        LONG total_sent, total_recv, verified_bytes;
        int recv_offset, i;
        LONG n;

        fd = helper_connect_service(HELPER_TCP_ECHO);
        if (fd >= 0) {
            set_recv_timeout(fd, 10);
            fill_test_pattern(sbuf, 8192, 0);

            /* Send 64KB (8 x 8KB) */
            total_sent = 0;
            for (i = 0; i < 8; i++) {
                n = send(fd, (UBYTE *)sbuf, 8192, 0);
                if (n <= 0) break;
                total_sent += n;
            }

            /* Receive with incremental chunk verification */
            total_recv = 0;
            verified_bytes = 0;
            recv_offset = 0;
            while (total_recv < 65536) {
                n = recv(fd, (UBYTE *)rbuf + recv_offset,
                         8192 - recv_offset, 0);
                if (n <= 0) break;
                total_recv += n;
                recv_offset += n;
                if (recv_offset >= 8192) {
                    if (verify_test_pattern(rbuf, 8192, 0) == 0)
                        verified_bytes += 8192;
                    recv_offset = 0;
                }
            }
            if (recv_offset > 0) {
                if (verify_test_pattern(rbuf, recv_offset, 0) == 0)
                    verified_bytes += recv_offset;
            }

            tap_ok(verified_bytes >= 65536,
                   "send()/recv(): 64KB TCP integrity via network [BSD 4.4]");
            tap_diagf("  sent=%ld recv=%ld verified=%ld",
                      (long)total_sent, (long)total_recv,
                      (long)verified_bytes);
            safe_close(fd);
        } else {
            tap_ok(0, "send()/recv(): 64KB TCP integrity via network [BSD 4.4]");
        }
    }

    CHECK_CTRLC();

    /* 40. udp_network_datagram */
    {
        LONG fd;
        struct sockaddr_in echo_addr;
        socklen_t fromlen;
        LONG n;

        fd = make_udp_socket();
        if (fd >= 0) {
            memset(&echo_addr, 0, sizeof(echo_addr));
            echo_addr.sin_family = AF_INET;
            echo_addr.sin_port = htons(HELPER_UDP_ECHO);
            echo_addr.sin_addr.s_addr = helper_addr();

            fill_test_pattern(sbuf, 512, 0x55);
            n = sendto(fd, (UBYTE *)sbuf, 512, 0,
                       (struct sockaddr *)&echo_addr, sizeof(echo_addr));

            if (n == 512) {
                set_recv_timeout(fd, 5);
                fromlen = sizeof(from_addr);
                n = recvfrom(fd, (UBYTE *)rbuf, sizeof(rbuf), 0,
                             (struct sockaddr *)&from_addr, &fromlen);
                if (n == 512) {
                    mismatch = verify_test_pattern(rbuf, 512, 0x55);
                    tap_ok(mismatch == 0,
                           "sendto()/recvfrom(): UDP datagram via network [RFC 768]");
                    tap_diagf("  sent=512 recv=%ld", (long)n);
                } else {
                    tap_ok(0, "sendto()/recvfrom(): UDP datagram via network [RFC 768]");
                    tap_diagf("  recv=%ld errno=%ld",
                              (long)n, (long)get_bsd_errno());
                }
            } else {
                tap_ok(0, "sendto()/recvfrom(): UDP datagram via network [RFC 768]");
                tap_diagf("  sendto=%ld errno=%ld",
                          (long)n, (long)get_bsd_errno());
            }
            safe_close(fd);
        } else {
            tap_ok(0, "sendto()/recvfrom(): UDP datagram via network [RFC 768]");
        }
    }

    CHECK_CTRLC();

    /* 41. accept_external */
    {
        LONG ext_listener, accepted;
        struct sockaddr_in bind_addr;
        fd_set readfds;
        struct timeval tv;
        LONG n;

        ext_listener = make_tcp_socket();
        if (ext_listener >= 0) {
            one = 1;
            setsockopt(ext_listener, SOL_SOCKET, SO_REUSEADDR,
                       &one, sizeof(one));
            memset(&bind_addr, 0, sizeof(bind_addr));
            bind_addr.sin_family = AF_INET;
            bind_addr.sin_port = htons(get_test_port(161));
            bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
            bind(ext_listener, (struct sockaddr *)&bind_addr,
                 sizeof(bind_addr));
            listen(ext_listener, 5);

            if (helper_request_connect(get_test_port(161))) {
                /* WaitSelect for incoming connection */
                FD_ZERO(&readfds);
                FD_SET(ext_listener, &readfds);
                tv.tv_secs = 5;
                tv.tv_micro = 0;
                rc = WaitSelect(ext_listener + 1, &readfds, NULL, NULL,
                                &tv, NULL);
                if (rc > 0) {
                    accepted = accept(ext_listener, NULL, NULL);
                    if (accepted >= 0) {
                        set_recv_timeout(accepted, 5);
                        total = 0;
                        while (total < 30) {
                            n = recv(accepted, (UBYTE *)rbuf + total,
                                     30 - total, 0);
                            if (n <= 0) break;
                            total += n;
                        }
                        tap_ok(total == 30 &&
                               memcmp(rbuf,
                                      "BSDSOCKTEST HELLO FROM HELPER\n",
                                      30) == 0,
                               "accept(): incoming connection from remote host [BSD 4.4]");
                        if (total != 30)
                            tap_diagf("  received %d of 30 bytes", total);
                        safe_close(accepted);
                    } else {
                        tap_ok(0, "accept(): incoming connection from remote host [BSD 4.4]");
                    }
                } else {
                    tap_ok(0, "accept(): incoming connection from remote host [BSD 4.4]");
                }
            } else {
                tap_ok(0, "accept(): incoming connection from remote host [BSD 4.4]");
            }
            safe_close(ext_listener);
        } else {
            tap_ok(0, "accept(): incoming connection from remote host [BSD 4.4]");
        }
    }

    CHECK_CTRLC();

    /* 42. tcp_network_large */
    {
        LONG fd;
        LONG total_sent, total_recv, verified_bytes;
        int recv_offset, i;
        LONG n;
        struct bst_timestamp ts_before, ts_after;
        LONG elapsed_ms, kbps;

        fd = helper_connect_service(HELPER_TCP_ECHO);
        if (fd >= 0) {
            set_recv_timeout(fd, 30);
            fill_test_pattern(sbuf, 8192, 0);

            timer_now(&ts_before);

            /* Send 256KB (32 x 8KB) */
            total_sent = 0;
            for (i = 0; i < 32; i++) {
                n = send(fd, (UBYTE *)sbuf, 8192, 0);
                if (n <= 0) break;
                total_sent += n;
            }

            /* Receive with incremental chunk verification */
            total_recv = 0;
            verified_bytes = 0;
            recv_offset = 0;
            while (total_recv < 262144) {
                n = recv(fd, (UBYTE *)rbuf + recv_offset,
                         8192 - recv_offset, 0);
                if (n <= 0) break;
                total_recv += n;
                recv_offset += n;
                if (recv_offset >= 8192) {
                    if (verify_test_pattern(rbuf, 8192, 0) == 0)
                        verified_bytes += 8192;
                    recv_offset = 0;
                }
            }
            if (recv_offset > 0) {
                if (verify_test_pattern(rbuf, recv_offset, 0) == 0)
                    verified_bytes += recv_offset;
            }

            timer_now(&ts_after);
            elapsed_ms = (LONG)timer_elapsed_ms(&ts_before, &ts_after);
            kbps = (elapsed_ms > 0)
                 ? (verified_bytes / 1024L) * 1000L / elapsed_ms
                 : 0;

            tap_ok(verified_bytes >= 262144,
                   "send()/recv(): 256KB+ TCP integrity via network [BSD 4.4]");
            tap_diagf("  sent=%ld recv=%ld verified=%ld ms=%ld KB/s=%ld",
                      (long)total_sent, (long)total_recv,
                      (long)verified_bytes, (long)elapsed_ms, (long)kbps);
            tap_notef("Network 256KB echo: %ld KB/s", (long)kbps);
            safe_close(fd);
        } else {
            tap_ok(0, "send()/recv(): 256KB+ TCP integrity via network [BSD 4.4]");
        }
    }
}
