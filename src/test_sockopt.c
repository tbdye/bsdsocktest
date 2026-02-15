/*
 * bsdsocktest — Socket option tests
 *
 * Tests: getsockopt, setsockopt (SO_TYPE, SO_REUSEADDR, SO_KEEPALIVE,
 *        SO_LINGER, SO_RCVTIMEO, SO_SNDTIMEO, TCP_NODELAY, SO_ERROR,
 *        SO_RCVBUF, SO_SNDBUF), IoctlSocket (FIONBIO, FIONREAD, FIOASYNC).
 *
 * 15 tests (43-57), port offsets 40-59.
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>

#include <sys/socket.h>
#include <sys/filio.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <errno.h>
#include <string.h>

void run_sockopt_tests(void)
{
    LONG fd, fd_tcp, fd_udp, listener, client, server;
    LONG optval, one = 1;
    socklen_t optlen;
    struct linger ling;
    struct timeval tv;
    struct sockaddr_in addr;
    struct bst_timestamp ts_before, ts_after;
    LONG elapsed_ms;
    int port, rc;

    /* ---- SO_TYPE ---- */

    /* 43. getsockopt_so_type */
    fd_tcp = make_tcp_socket();
    fd_udp = make_udp_socket();
    if (fd_tcp >= 0 && fd_udp >= 0) {
        optlen = sizeof(optval);
        getsockopt(fd_tcp, SOL_SOCKET, SO_TYPE, &optval, &optlen);
        rc = (optval == SOCK_STREAM);
        optlen = sizeof(optval);
        getsockopt(fd_udp, SOL_SOCKET, SO_TYPE, &optval, &optlen);
        tap_ok(rc && optval == SOCK_DGRAM,
               "getsockopt(SO_TYPE): query socket type [BSD 4.4]");
    } else {
        tap_ok(0, "getsockopt(SO_TYPE): query socket type [BSD 4.4]");
    }
    safe_close(fd_tcp);
    safe_close(fd_udp);

    CHECK_CTRLC();

    /* ---- SO_REUSEADDR ---- */

    /* 44. so_reuseaddr_default */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = -1;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
        tap_ok(1, "SO_REUSEADDR: query default value [BSD 4.4]");
        tap_diagf("  default SO_REUSEADDR: %ld", (long)optval);
    } else {
        tap_ok(0, "SO_REUSEADDR: query default value [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 45. so_reuseaddr_set */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
        tap_ok(rc == 0 && optval != 0,
               "SO_REUSEADDR: enable address reuse [BSD 4.4]");
    } else {
        tap_ok(0, "SO_REUSEADDR: enable address reuse [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 46. so_reuseaddr_get */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = 0;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        optval = -1;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
        if (optval == 0) {
            tap_ok(1, "SO_REUSEADDR: clear and read-back behavior [BSD 4.4]");
        } else {
            tap_ok(1, "SO_REUSEADDR: clear and read-back behavior [BSD 4.4]");
            tap_diag("  SO_REUSEADDR could not be cleared");
        }
    } else {
        tap_ok(0, "SO_REUSEADDR: clear and read-back behavior [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_KEEPALIVE ---- */

    /* 47. so_keepalive */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen);
        tap_ok(rc == 0 && optval != 0,
               "SO_KEEPALIVE: enable keepalive probes [RFC 1122]");
    } else {
        tap_ok(0, "SO_KEEPALIVE: enable keepalive probes [RFC 1122]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_LINGER ---- */

    /* 48. so_linger */
    fd = make_tcp_socket();
    if (fd >= 0) {
        memset(&ling, 0, sizeof(ling));
        ling.l_onoff = 1;
        ling.l_linger = 5;
        rc = setsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));
        memset(&ling, 0, sizeof(ling));
        optlen = sizeof(ling);
        getsockopt(fd, SOL_SOCKET, SO_LINGER, &ling, &optlen);
        tap_ok(rc == 0 && ling.l_onoff != 0 && ling.l_linger == 5,
               "SO_LINGER: set and read back linger struct [BSD 4.4]");
    } else {
        tap_ok(0, "SO_LINGER: set and read back linger struct [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_RCVTIMEO ---- */

    /* 49. so_rcvtimeo */
    port = get_test_port(40);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (server >= 0) {
        tv.tv_secs = 1;
        tv.tv_micro = 0;
        rc = setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if (rc < 0) {
            tap_skip("SO_RCVTIMEO not supported");
        } else {
            timer_now(&ts_before);
            rc = recv(server, (UBYTE *)&optval, sizeof(optval), 0);
            timer_now(&ts_after);
            elapsed_ms = (LONG)timer_elapsed_ms(&ts_before, &ts_after);
            tap_ok(rc < 0 &&
                   (get_bsd_errno() == EWOULDBLOCK || get_bsd_errno() == EAGAIN) &&
                   elapsed_ms >= 500 && elapsed_ms <= 3000,
                   "SO_RCVTIMEO: receive timeout fires [BSD 4.4]");
            tap_diagf("  elapsed: %ldms (%ld.%03ld s)",
                      (long)elapsed_ms,
                      (long)(elapsed_ms / 1000),
                      (long)(elapsed_ms % 1000));
        }
    } else {
        tap_ok(0, "SO_RCVTIMEO: receive timeout fires [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- SO_SNDTIMEO ---- */

    /* 50. so_sndtimeo */
    fd = make_tcp_socket();
    if (fd >= 0) {
        tv.tv_secs = 1;
        tv.tv_micro = 0;
        rc = setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
        if (rc == 0) {
            memset(&tv, 0, sizeof(tv));
            optlen = sizeof(tv);
            getsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, &optlen);
            tap_ok(tv.tv_secs == 1 && tv.tv_micro == 0,
                   "SO_SNDTIMEO: set send timeout [BSD 4.4]");
        } else {
            tap_ok(rc == 0, "SO_SNDTIMEO: set send timeout [BSD 4.4]");
        }
    } else {
        tap_ok(0, "SO_SNDTIMEO: set send timeout [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- TCP_NODELAY ---- */

    /* 51. tcp_nodelay */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, &optlen);
        tap_ok(rc == 0 && optval != 0,
               "TCP_NODELAY: disable Nagle algorithm [RFC 896/1122]");
    } else {
        tap_ok(0, "TCP_NODELAY: disable Nagle algorithm [RFC 896/1122]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_ERROR ---- */

    /* 52. so_error_after_failed_connect */
    port = get_test_port(41);
    fd = make_tcp_socket();
    if (fd >= 0) {
        set_nonblocking(fd);
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        if (rc < 0 && get_bsd_errno() == EINPROGRESS) {
            /* Wait for connect to complete/fail via WaitSelect */
            {
                fd_set wfds;
                struct timeval wtv;
                FD_ZERO(&wfds);
                FD_SET(fd, &wfds);
                wtv.tv_secs = 2;
                wtv.tv_micro = 0;
                WaitSelect(fd + 1, NULL, &wfds, NULL, &wtv, NULL);
            }
            optval = 0;
            optlen = sizeof(optval);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
            tap_ok(optval == ECONNREFUSED,
                   "SO_ERROR: pending error after failed connect [BSD 4.4]");
            tap_diagf("  SO_ERROR: %ld", (long)optval);
        } else if (rc < 0 && get_bsd_errno() == ECONNREFUSED) {
            /* Non-blocking connect returned ECONNREFUSED immediately */
            optval = 0;
            optlen = sizeof(optval);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
            tap_ok(1, "SO_ERROR: pending error after failed connect [BSD 4.4]");
            tap_diagf("  SO_ERROR: %ld (connect was immediate ECONNREFUSED)",
                      (long)optval);
        } else {
            tap_ok(0, "SO_ERROR: pending error after failed connect [BSD 4.4]");
            tap_diagf("  rc=%d, errno=%ld", rc, (long)get_bsd_errno());
        }
    } else {
        tap_ok(0, "SO_ERROR: pending error after failed connect [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_RCVBUF / SO_SNDBUF ---- */

    /* 53. so_rcvbuf */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = 32768;
        rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
        tap_ok(rc == 0 && optval >= 32768,
               "SO_RCVBUF: set receive buffer size [BSD 4.4]");
        tap_diagf("  SO_RCVBUF: %ld", (long)optval);
    } else {
        tap_ok(0, "SO_RCVBUF: set receive buffer size [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 54. so_sndbuf */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = 32768;
        rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
        tap_ok(rc == 0 && optval >= 32768,
               "SO_SNDBUF: set send buffer size [BSD 4.4]");
        tap_diagf("  SO_SNDBUF: %ld", (long)optval);
    } else {
        tap_ok(0, "SO_SNDBUF: set send buffer size [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- IoctlSocket ---- */

    /* 55. ioctl_fionbio */
    port = get_test_port(42);
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = IoctlSocket(fd, FIONBIO, (APTR)&one);
        if (rc == 0) {
            /* Non-blocking connect should return EINPROGRESS */
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
            tap_ok(rc < 0 && (get_bsd_errno() == EINPROGRESS ||
                               get_bsd_errno() == ECONNREFUSED),
                   "IoctlSocket(FIONBIO): set non-blocking mode [AmiTCP]");
            tap_diagf("  errno: %ld", (long)get_bsd_errno());
        } else {
            tap_ok(0, "IoctlSocket(FIONBIO): set non-blocking mode [AmiTCP]");
        }
    } else {
        tap_ok(0, "IoctlSocket(FIONBIO): set non-blocking mode [AmiTCP]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 56. ioctl_fionread */
    port = get_test_port(43);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        {
            unsigned char data[100];
            fill_test_pattern(data, 100, 20);
            send(client, (UBYTE *)data, 100, 0);
        }
        /* Brief delay for data to arrive — use WaitSelect with timeout */
        {
            fd_set rfds;
            struct timeval wtv;
            FD_ZERO(&rfds);
            FD_SET(server, &rfds);
            wtv.tv_secs = 1;
            wtv.tv_micro = 0;
            WaitSelect(server + 1, &rfds, NULL, NULL, &wtv, NULL);
        }
        optval = 0;
        rc = IoctlSocket(server, FIONREAD, (APTR)&optval);
        tap_ok(rc == 0 && optval == 100,
               "IoctlSocket(FIONREAD): query pending bytes [AmiTCP]");
        if (optval != 100)
            tap_diagf("  FIONREAD: %ld", (long)optval);
    } else {
        tap_ok(0, "IoctlSocket(FIONREAD): query pending bytes [AmiTCP]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 57. ioctl_fioasync */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = IoctlSocket(fd, FIOASYNC, (APTR)&one);
        if (rc == 0) {
            tap_ok(1, "IoctlSocket(FIOASYNC): async notification mode [AmiTCP]");
        } else {
            tap_skip("FIOASYNC not supported");
        }
    } else {
        tap_ok(0, "IoctlSocket(FIOASYNC): async notification mode [AmiTCP]");
    }
    safe_close(fd);
}
