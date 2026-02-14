/*
 * bsdsocktest — Throughput benchmark tests
 *
 * Tests: TCP/UDP throughput measurement (loopback + network).
 * Results reported as TAP diagnostics. Tests pass as long as data
 * was transferred; throughput numbers are informational.
 *
 * 6 tests (136-141), port offsets 180-199.
 */

#include "tap.h"
#include "testutil.h"
#include "helper_proto.h"

#include <proto/bsdsocket.h>
#include <proto/dos.h>

#include <netinet/in.h>
#include <string.h>

#define TP_BUFSIZE      8192            /* 8KB send/recv buffer */
#define TP_TCP_BYTES    (512L * 1024)   /* 512KB for standard tests */
#define TP_SUSTAINED    (1024L * 1024)  /* 1MB for sustained tests */
#define TP_UDP_COUNT    200             /* UDP datagrams to send */
#define TP_UDP_SIZE     1024            /* 1KB per UDP datagram */

#define TP_SEGMENT_SIZE (100L * 1024)
#define TP_NUM_SEGMENTS 10

static unsigned char tp_sbuf[TP_BUFSIZE];
static unsigned char tp_rbuf[TP_BUFSIZE];

static LONG elapsed_ms(struct DateStamp *before, struct DateStamp *after)
{
    LONG ticks;

    ticks = (after->ds_Days - before->ds_Days) * 24L * 60 * 50 * 60
          + (after->ds_Minute - before->ds_Minute) * 50L * 60
          + (after->ds_Tick - before->ds_Tick);
    return ticks * 20;  /* 1 tick = 20ms */
}

void run_throughput_tests(void)
{
    LONG listener, client, server;
    int port;
    LONG total_sent, total_recv;
    int send_done;
    LONG maxfd, rc, n, chunk;
    fd_set readfds, writefds;
    struct timeval tv;
    struct DateStamp before, after;
    LONG ms, kbps;

    fill_test_pattern(tp_sbuf, TP_BUFSIZE, 0);

    /* ---- 136. tp_tcp_loopback ---- */
    port = get_test_port(180);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        set_nonblocking(client);
        set_nonblocking(server);

        total_sent = 0;
        total_recv = 0;
        send_done = 0;

        DateStamp(&before);
        while (total_recv < TP_TCP_BYTES) {
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_SET(server, &readfds);
            if (!send_done)
                FD_SET(client, &writefds);

            maxfd = (client > server ? client : server) + 1;
            tv.tv_secs = 10;
            tv.tv_micro = 0;
            rc = WaitSelect(maxfd, &readfds, &writefds, NULL, &tv, NULL);
            if (rc <= 0)
                break;

            if (!send_done && FD_ISSET(client, &writefds)) {
                chunk = TP_TCP_BYTES - total_sent;
                if (chunk > TP_BUFSIZE) chunk = TP_BUFSIZE;
                n = send(client, (UBYTE *)tp_sbuf, chunk, 0);
                if (n > 0) total_sent += n;
                if (total_sent >= TP_TCP_BYTES) {
                    shutdown(client, 1);  /* SHUT_WR */
                    send_done = 1;
                }
            }
            if (FD_ISSET(server, &readfds)) {
                n = recv(server, (UBYTE *)tp_rbuf, TP_BUFSIZE, 0);
                if (n > 0) total_recv += n;
                else if (n == 0) break;  /* EOF */
            }
        }
        DateStamp(&after);

        ms = elapsed_ms(&before, &after);
        kbps = (ms > 0) ? (total_recv / 1024L) * 1000L / ms : 0;
        tap_ok(total_recv >= TP_TCP_BYTES * 90 / 100,
               "tp_tcp_loopback - 512KB TCP loopback transfer");
        tap_diagf("  sent=%ld recv=%ld ms=%ld KB/s=%ld",
                  (long)total_sent, (long)total_recv, (long)ms, (long)kbps);
    } else {
        tap_ok(0, "tp_tcp_loopback - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- 137. tp_tcp_network ---- */
    if (!helper_is_connected()) {
        tap_skip("tp_tcp_network - host helper not connected");
    } else {
        LONG fd;

        fd = helper_connect_service(HELPER_TCP_SINK);
        if (fd >= 0) {
            total_sent = 0;
            DateStamp(&before);
            while (total_sent < TP_TCP_BYTES) {
                chunk = TP_TCP_BYTES - total_sent;
                if (chunk > TP_BUFSIZE) chunk = TP_BUFSIZE;
                n = send(fd, (UBYTE *)tp_sbuf, chunk, 0);
                if (n <= 0) break;
                total_sent += n;
            }
            DateStamp(&after);

            ms = elapsed_ms(&before, &after);
            kbps = (ms > 0) ? (total_sent / 1024L) * 1000L / ms : 0;
            tap_ok(total_sent > 0,
                   "tp_tcp_network - 512KB TCP to helper sink");
            tap_diagf("  sent=%ld ms=%ld KB/s=%ld",
                      (long)total_sent, (long)ms, (long)kbps);
            safe_close(fd);
        } else {
            tap_ok(0, "tp_tcp_network - could not connect to sink");
        }
    }

    CHECK_CTRLC();

    /* ---- 138. tp_udp_loopback ---- */
    {
        LONG sock_a, sock_b;
        struct sockaddr_in addr_a, addr_b;
        int i, received;

        sock_a = make_udp_socket();
        sock_b = make_udp_socket();
        if (sock_a >= 0 && sock_b >= 0) {
            memset(&addr_a, 0, sizeof(addr_a));
            addr_a.sin_family = AF_INET;
            addr_a.sin_port = htons(get_test_port(181));
            addr_a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            bind(sock_a, (struct sockaddr *)&addr_a, sizeof(addr_a));

            memset(&addr_b, 0, sizeof(addr_b));
            addr_b.sin_family = AF_INET;
            addr_b.sin_port = htons(get_test_port(182));
            addr_b.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            bind(sock_b, (struct sockaddr *)&addr_b, sizeof(addr_b));

            DateStamp(&before);
            for (i = 0; i < TP_UDP_COUNT; i++) {
                fill_test_pattern(tp_sbuf, TP_UDP_SIZE, i);
                sendto(sock_a, (UBYTE *)tp_sbuf, TP_UDP_SIZE, 0,
                       (struct sockaddr *)&addr_b, sizeof(addr_b));
            }

            /* Recv all available (WaitSelect for readability) */
            set_nonblocking(sock_b);
            received = 0;
            {
                fd_set rdfds;

                while (1) {
                    FD_ZERO(&rdfds);
                    FD_SET(sock_b, &rdfds);
                    tv.tv_secs = 1;
                    tv.tv_micro = 0;
                    rc = WaitSelect(sock_b + 1, &rdfds, NULL, NULL,
                                    &tv, NULL);
                    if (rc <= 0) break;
                    while (1) {
                        n = recv(sock_b, (UBYTE *)tp_rbuf, TP_BUFSIZE, 0);
                        if (n <= 0) break;
                        received++;
                    }
                }
            }
            DateStamp(&after);

            ms = elapsed_ms(&before, &after);
            kbps = (ms > 0)
                 ? ((long)received * TP_UDP_SIZE / 1024L) * 1000L / ms
                 : 0;
            tap_ok(received > 0,
                   "tp_udp_loopback - UDP loopback datagram throughput");
            tap_diagf("  sent=%d recv=%d loss=%ld%% ms=%ld KB/s=%ld",
                      TP_UDP_COUNT, received,
                      (long)(TP_UDP_COUNT - received) * 100 / TP_UDP_COUNT,
                      (long)ms, (long)kbps);
        } else {
            tap_ok(0, "tp_udp_loopback - could not create UDP sockets");
        }
        safe_close(sock_a);
        safe_close(sock_b);
    }

    CHECK_CTRLC();

    /* ---- 139. tp_udp_network ---- */
    if (!helper_is_connected()) {
        tap_skip("tp_udp_network - host helper not connected");
    } else {
        LONG fd;
        struct sockaddr_in echo_addr;
        int i, received;

        fd = make_udp_socket();
        if (fd >= 0) {
            memset(&echo_addr, 0, sizeof(echo_addr));
            echo_addr.sin_family = AF_INET;
            echo_addr.sin_port = htons(HELPER_UDP_ECHO);
            echo_addr.sin_addr.s_addr = helper_addr();

            DateStamp(&before);
            for (i = 0; i < TP_UDP_COUNT; i++) {
                fill_test_pattern(tp_sbuf, TP_UDP_SIZE, i);
                sendto(fd, (UBYTE *)tp_sbuf, TP_UDP_SIZE, 0,
                       (struct sockaddr *)&echo_addr, sizeof(echo_addr));
            }

            /* Wait briefly then recv echoed replies */
            received = 0;
            set_nonblocking(fd);
            {
                fd_set rdfds;

                while (1) {
                    FD_ZERO(&rdfds);
                    FD_SET(fd, &rdfds);
                    tv.tv_secs = 1;
                    tv.tv_micro = 0;
                    rc = WaitSelect(fd + 1, &rdfds, NULL, NULL, &tv, NULL);
                    if (rc <= 0) break;
                    while (1) {
                        n = recv(fd, (UBYTE *)tp_rbuf, TP_BUFSIZE, 0);
                        if (n <= 0) break;
                        received++;
                    }
                }
            }
            DateStamp(&after);

            ms = elapsed_ms(&before, &after);
            kbps = (ms > 0)
                 ? ((long)received * TP_UDP_SIZE / 1024L) * 1000L / ms
                 : 0;
            tap_ok(received > 0,
                   "tp_udp_network - UDP network echo throughput");
            tap_diagf("  sent=%d echoed=%d loss=%ld%% ms=%ld KB/s=%ld",
                      TP_UDP_COUNT, received,
                      (long)(TP_UDP_COUNT - received) * 100 / TP_UDP_COUNT,
                      (long)ms, (long)kbps);
            safe_close(fd);
        } else {
            tap_ok(0, "tp_udp_network - could not create UDP socket");
        }
    }

    CHECK_CTRLC();

    /* ---- 140. tp_tcp_sustained_loopback ---- */
    port = get_test_port(183);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        LONG seg_ms[TP_NUM_SEGMENTS];
        int cur_seg;
        struct DateStamp seg_start, seg_now, total_before, total_after;
        LONG seg_kbps;

        set_nonblocking(client);
        set_nonblocking(server);

        total_sent = 0;
        total_recv = 0;
        send_done = 0;
        cur_seg = 0;

        DateStamp(&total_before);
        DateStamp(&seg_start);

        while (total_recv < TP_SUSTAINED) {
            FD_ZERO(&readfds);
            FD_ZERO(&writefds);
            FD_SET(server, &readfds);
            if (!send_done)
                FD_SET(client, &writefds);

            maxfd = (client > server ? client : server) + 1;
            tv.tv_secs = 10;
            tv.tv_micro = 0;
            rc = WaitSelect(maxfd, &readfds, &writefds, NULL, &tv, NULL);
            if (rc <= 0)
                break;

            if (!send_done && FD_ISSET(client, &writefds)) {
                chunk = TP_SUSTAINED - total_sent;
                if (chunk > TP_BUFSIZE) chunk = TP_BUFSIZE;
                n = send(client, (UBYTE *)tp_sbuf, chunk, 0);
                if (n > 0) {
                    total_sent += n;
                    /* Checkpoint at segment boundaries */
                    while (total_sent >= (cur_seg + 1) * TP_SEGMENT_SIZE &&
                           cur_seg < TP_NUM_SEGMENTS) {
                        DateStamp(&seg_now);
                        seg_ms[cur_seg] = elapsed_ms(&seg_start, &seg_now);
                        seg_start = seg_now;
                        cur_seg++;
                    }
                }
                if (total_sent >= TP_SUSTAINED) {
                    shutdown(client, 1);
                    send_done = 1;
                }
            }
            if (FD_ISSET(server, &readfds)) {
                n = recv(server, (UBYTE *)tp_rbuf, TP_BUFSIZE, 0);
                if (n > 0) total_recv += n;
                else if (n == 0) break;
            }
        }
        DateStamp(&total_after);

        ms = elapsed_ms(&total_before, &total_after);
        kbps = (ms > 0) ? (total_recv / 1024L) * 1000L / ms : 0;
        tap_ok(total_recv >= TP_SUSTAINED,
               "tp_tcp_sustained_loopback - 1MB sustained TCP loopback");
        tap_diagf("  sent=%ld recv=%ld total_ms=%ld overall_KB/s=%ld",
                  (long)total_sent, (long)total_recv, (long)ms, (long)kbps);

        /* Per-segment diagnostics */
        if (cur_seg > 0) {
            LONG seg_min, seg_max;
            int si;

            seg_min = seg_ms[0];
            seg_max = seg_ms[0];
            for (si = 1; si < cur_seg; si++) {
                if (seg_ms[si] < seg_min) seg_min = seg_ms[si];
                if (seg_ms[si] > seg_max) seg_max = seg_ms[si];
            }
            tap_diagf("  segments=%d seg_min=%ldms seg_max=%ldms",
                      cur_seg, (long)seg_min, (long)seg_max);
            for (si = 0; si < cur_seg; si++) {
                seg_kbps = (seg_ms[si] > 0)
                         ? (TP_SEGMENT_SIZE / 1024L) * 1000L / seg_ms[si]
                         : 0;
                tap_diagf("    seg[%d]: %ldms %ldKB/s",
                          si, (long)seg_ms[si], (long)seg_kbps);
            }
        }
    } else {
        tap_ok(0, "tp_tcp_sustained_loopback - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- 141. tp_tcp_sustained_network ---- */
    if (!helper_is_connected()) {
        tap_skip("tp_tcp_sustained_network - host helper not connected");
    } else {
        LONG fd;
        LONG seg_ms[TP_NUM_SEGMENTS];
        int cur_seg;
        struct DateStamp seg_start, seg_now, total_before, total_after;
        LONG seg_kbps;

        fd = helper_connect_service(HELPER_TCP_SINK);
        if (fd >= 0) {
            total_sent = 0;
            cur_seg = 0;

            DateStamp(&total_before);
            DateStamp(&seg_start);

            while (total_sent < TP_SUSTAINED) {
                chunk = TP_SUSTAINED - total_sent;
                if (chunk > TP_BUFSIZE) chunk = TP_BUFSIZE;
                n = send(fd, (UBYTE *)tp_sbuf, chunk, 0);
                if (n <= 0) break;
                total_sent += n;

                /* Checkpoint at segment boundaries */
                while (total_sent >= (cur_seg + 1) * TP_SEGMENT_SIZE &&
                       cur_seg < TP_NUM_SEGMENTS) {
                    DateStamp(&seg_now);
                    seg_ms[cur_seg] = elapsed_ms(&seg_start, &seg_now);
                    seg_start = seg_now;
                    cur_seg++;
                }
            }
            DateStamp(&total_after);

            ms = elapsed_ms(&total_before, &total_after);
            kbps = (ms > 0) ? (total_sent / 1024L) * 1000L / ms : 0;
            tap_ok(total_sent >= TP_SUSTAINED,
                   "tp_tcp_sustained_network - 1MB sustained TCP to sink");
            tap_diagf("  sent=%ld total_ms=%ld overall_KB/s=%ld",
                      (long)total_sent, (long)ms, (long)kbps);

            /* Per-segment diagnostics */
            if (cur_seg > 0) {
                LONG seg_min, seg_max;
                int si;

                seg_min = seg_ms[0];
                seg_max = seg_ms[0];
                for (si = 1; si < cur_seg; si++) {
                    if (seg_ms[si] < seg_min) seg_min = seg_ms[si];
                    if (seg_ms[si] > seg_max) seg_max = seg_ms[si];
                }
                tap_diagf("  segments=%d seg_min=%ldms seg_max=%ldms",
                          cur_seg, (long)seg_min, (long)seg_max);
                for (si = 0; si < cur_seg; si++) {
                    seg_kbps = (seg_ms[si] > 0)
                             ? (TP_SEGMENT_SIZE / 1024L) * 1000L / seg_ms[si]
                             : 0;
                    tap_diagf("    seg[%d]: %ldms %ldKB/s",
                              si, (long)seg_ms[si], (long)seg_kbps);
                }
            }
            safe_close(fd);
        } else {
            tap_ok(0, "tp_tcp_sustained_network - could not connect to sink");
        }
    }
}
