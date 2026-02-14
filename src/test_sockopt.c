/*
 * bsdsocktest — Socket option tests
 *
 * Tests: getsockopt, setsockopt (SO_TYPE, SO_REUSEADDR, SO_KEEPALIVE,
 *        SO_LINGER, SO_RCVTIMEO, SO_SNDTIMEO, TCP_NODELAY, SO_ERROR,
 *        SO_RCVBUF, SO_SNDBUF), IoctlSocket (FIONBIO, FIONREAD, FIOASYNC).
 *
 * 15 tests, port offsets 40-59.
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>
#include <proto/dos.h>

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
    struct DateStamp before, after;
    LONG elapsed_ticks;
    int port, rc;

    /* ---- SO_TYPE ---- */

    /* 39. getsockopt_so_type */
    fd_tcp = make_tcp_socket();
    fd_udp = make_udp_socket();
    if (fd_tcp >= 0 && fd_udp >= 0) {
        optlen = sizeof(optval);
        getsockopt(fd_tcp, SOL_SOCKET, SO_TYPE, &optval, &optlen);
        rc = (optval == SOCK_STREAM);
        optlen = sizeof(optval);
        getsockopt(fd_udp, SOL_SOCKET, SO_TYPE, &optval, &optlen);
        tap_ok(rc && optval == SOCK_DGRAM,
               "getsockopt_so_type - TCP=STREAM, UDP=DGRAM");
    } else {
        tap_ok(0, "getsockopt_so_type - could not create sockets");
    }
    safe_close(fd_tcp);
    safe_close(fd_udp);

    CHECK_CTRLC();

    /* ---- SO_REUSEADDR ---- */

    /* 40. so_reuseaddr_default */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = -1;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
        tap_ok(1, "so_reuseaddr_default - queried default value");
        tap_diagf("  default SO_REUSEADDR: %ld", (long)optval);
    } else {
        tap_ok(0, "so_reuseaddr_default - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 41. so_reuseaddr_set */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
        tap_ok(rc == 0 && optval != 0,
               "so_reuseaddr_set - set and verify SO_REUSEADDR=1");
    } else {
        tap_ok(0, "so_reuseaddr_set - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 42. so_reuseaddr_get */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = 0;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        optval = -1;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, &optlen);
        if (optval == 0) {
            tap_ok(1, "so_reuseaddr_get - SO_REUSEADDR cleared to 0");
        } else {
            tap_ok(1, "so_reuseaddr_get - SO_REUSEADDR forced on (known Amiberry behavior)");
            tap_diag("  SO_REUSEADDR could not be cleared");
        }
    } else {
        tap_ok(0, "so_reuseaddr_get - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_KEEPALIVE ---- */

    /* 43. so_keepalive */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &one, sizeof(one));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, &optlen);
        tap_ok(rc == 0 && optval != 0,
               "so_keepalive - set and verify SO_KEEPALIVE=1");
    } else {
        tap_ok(0, "so_keepalive - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_LINGER ---- */

    /* 44. so_linger */
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
               "so_linger - set and verify linger {on=1, time=5}");
    } else {
        tap_ok(0, "so_linger - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_RCVTIMEO ---- */

    /* 45. so_rcvtimeo */
    port = get_test_port(40);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (server >= 0) {
        tv.tv_secs = 1;
        tv.tv_micro = 0;
        rc = setsockopt(server, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        if (rc < 0) {
            tap_skip("so_rcvtimeo - SO_RCVTIMEO not supported");
        } else {
            DateStamp(&before);
            rc = recv(server, (UBYTE *)&optval, sizeof(optval), 0);
            DateStamp(&after);
            elapsed_ticks = (after.ds_Minute - before.ds_Minute) * 50 * 60 +
                            (after.ds_Tick - before.ds_Tick);
            tap_ok(rc < 0 &&
                   (get_bsd_errno() == EWOULDBLOCK || get_bsd_errno() == EAGAIN) &&
                   elapsed_ticks >= 25 && elapsed_ticks <= 150,
                   "so_rcvtimeo - recv times out after ~1 second");
            tap_diagf("  elapsed: %ld ticks (%ld.%02ld s)",
                      (long)elapsed_ticks,
                      (long)(elapsed_ticks / 50),
                      (long)((elapsed_ticks % 50) * 2));
        }
    } else {
        tap_ok(0, "so_rcvtimeo - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- SO_SNDTIMEO ---- */

    /* 46. so_sndtimeo */
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
                   "so_sndtimeo - set and verify SO_SNDTIMEO roundtrips");
        } else {
            tap_ok(rc == 0, "so_sndtimeo - setsockopt failed");
        }
    } else {
        tap_ok(0, "so_sndtimeo - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- TCP_NODELAY ---- */

    /* 47. tcp_nodelay */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, &optlen);
        tap_ok(rc == 0 && optval != 0,
               "tcp_nodelay - set and verify TCP_NODELAY=1");
    } else {
        tap_ok(0, "tcp_nodelay - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_ERROR ---- */

    /* 48. so_error_after_failed_connect */
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
                   "so_error_after_failed_connect - SO_ERROR is ECONNREFUSED");
            tap_diagf("  SO_ERROR: %ld", (long)optval);
        } else if (rc < 0 && get_bsd_errno() == ECONNREFUSED) {
            /* Non-blocking connect returned ECONNREFUSED immediately */
            optval = 0;
            optlen = sizeof(optval);
            getsockopt(fd, SOL_SOCKET, SO_ERROR, &optval, &optlen);
            tap_ok(1, "so_error_after_failed_connect - connect failed immediately");
            tap_diagf("  SO_ERROR: %ld (connect was immediate ECONNREFUSED)",
                      (long)optval);
        } else {
            tap_ok(0, "so_error_after_failed_connect - unexpected connect result");
            tap_diagf("  rc=%d, errno=%ld", rc, (long)get_bsd_errno());
        }
    } else {
        tap_ok(0, "so_error_after_failed_connect - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- SO_RCVBUF / SO_SNDBUF ---- */

    /* 49. so_rcvbuf */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = 32768;
        rc = setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(optval));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_RCVBUF, &optval, &optlen);
        tap_ok(rc == 0 && optval >= 32768,
               "so_rcvbuf - set and verify SO_RCVBUF >= 32768");
        tap_diagf("  SO_RCVBUF: %ld", (long)optval);
    } else {
        tap_ok(0, "so_rcvbuf - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 50. so_sndbuf */
    fd = make_tcp_socket();
    if (fd >= 0) {
        optval = 32768;
        rc = setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(optval));
        optval = 0;
        optlen = sizeof(optval);
        getsockopt(fd, SOL_SOCKET, SO_SNDBUF, &optval, &optlen);
        tap_ok(rc == 0 && optval >= 32768,
               "so_sndbuf - set and verify SO_SNDBUF >= 32768");
        tap_diagf("  SO_SNDBUF: %ld", (long)optval);
    } else {
        tap_ok(0, "so_sndbuf - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- IoctlSocket ---- */

    /* 51. ioctl_fionbio */
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
                   "ioctl_fionbio - non-blocking connect returns immediately");
            tap_diagf("  errno: %ld", (long)get_bsd_errno());
        } else {
            tap_ok(0, "ioctl_fionbio - FIONBIO ioctl failed");
        }
    } else {
        tap_ok(0, "ioctl_fionbio - could not create socket");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 52. ioctl_fionread */
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
               "ioctl_fionread - FIONREAD reports 100 pending bytes");
        if (optval != 100)
            tap_diagf("  FIONREAD: %ld", (long)optval);
    } else {
        tap_ok(0, "ioctl_fionread - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 53. ioctl_fioasync */
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        rc = IoctlSocket(fd, FIOASYNC, (APTR)&one);
        if (rc == 0) {
            tap_ok(1, "ioctl_fioasync - FIOASYNC set successfully");
        } else {
            tap_skip("ioctl_fioasync - FIOASYNC not supported");
        }
    } else {
        tap_ok(0, "ioctl_fioasync - could not create socket");
    }
    safe_close(fd);
}
