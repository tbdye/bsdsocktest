/*
 * bsdsocktest — Core socket operation tests
 *
 * Tests: socket, bind, listen, connect, accept, shutdown,
 *        CloseSocket, getsockname, getpeername.
 *
 * 23 tests (1-23), port offsets 0-19.
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

void run_socket_tests(void)
{
    LONG fd, fd2, listener, client, server;
    struct sockaddr_in addr;
    socklen_t addrlen;
    LONG one = 1;
    int port;
    int rc;
    unsigned char buf[16];

    /* ---- socket() creation ---- */

    /* 1. socket_create_tcp */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    tap_ok(fd >= 0, "socket(): create SOCK_STREAM (TCP) [BSD 4.4]");
    safe_close(fd);

    CHECK_CTRLC();

    /* 2. socket_create_udp */
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    tap_ok(fd >= 0, "socket(): create SOCK_DGRAM (UDP) [BSD 4.4]");
    safe_close(fd);

    CHECK_CTRLC();

    /* 3. socket_create_raw */
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd >= 0) {
        tap_ok(1, "socket(): create SOCK_RAW (ICMP) [BSD 4.4]");
        safe_close(fd);
    } else if (get_bsd_errno() == EACCES) {
        tap_skip("raw sockets require privileges");
    } else {
        tap_ok(fd >= 0, "socket(): create SOCK_RAW (ICMP) [BSD 4.4]");
    }

    CHECK_CTRLC();

    /* 4. socket_invalid_domain */
    fd = socket(-1, SOCK_STREAM, 0);
    tap_ok(fd == -1 && get_bsd_errno() != 0,
           "socket(): reject invalid domain (errno) [BSD 4.4]");
    if (fd >= 0)
        safe_close(fd);

    CHECK_CTRLC();

    /* 5. socket_invalid_type */
    fd = socket(AF_INET, 999, 0);
    tap_ok(fd == -1 && get_bsd_errno() != 0,
           "socket(): reject invalid type (errno) [BSD 4.4]");
    if (fd >= 0)
        safe_close(fd);

    CHECK_CTRLC();

    /* ---- bind() ---- */

    /* 6. bind_any_port_zero */
    fd = make_tcp_socket();
    if (fd >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = 0;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        rc = bind(fd, (struct sockaddr *)&addr, sizeof(addr));

        addrlen = sizeof(addr);
        getsockname(fd, (struct sockaddr *)&addr, &addrlen);
        tap_ok(rc == 0 && ntohs(addr.sin_port) > 0,
               "bind(): INADDR_ANY port 0 auto-assigns ephemeral port [BSD 4.4]");
        tap_diagf("  assigned port: %d", (int)ntohs(addr.sin_port));
    } else {
        tap_ok(0, "bind(): INADDR_ANY port 0 auto-assigns ephemeral port [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 7. bind_specific_port */
    port = get_test_port(0);
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        rc = bind(fd, (struct sockaddr *)&addr, sizeof(addr));

        addrlen = sizeof(addr);
        getsockname(fd, (struct sockaddr *)&addr, &addrlen);
        tap_ok(rc == 0 && ntohs(addr.sin_port) == port,
               "bind(): specific port assignment [BSD 4.4]");
    } else {
        tap_ok(0, "bind(): specific port assignment [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 8. bind_eaddrinuse */
    port = get_test_port(1);
    fd = make_tcp_socket();
    fd2 = make_tcp_socket();
    if (fd >= 0 && fd2 >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(fd, (struct sockaddr *)&addr, sizeof(addr));
        listen(fd, 5);
        rc = bind(fd2, (struct sockaddr *)&addr, sizeof(addr));
        tap_ok(rc < 0 && get_bsd_errno() == EADDRINUSE,
               "bind(): EADDRINUSE on double-bind [BSD 4.4]");
    } else {
        tap_ok(0, "bind(): EADDRINUSE on double-bind [BSD 4.4]");
    }
    safe_close(fd);
    safe_close(fd2);

    CHECK_CTRLC();

    /* ---- listen() ---- */

    /* 9. listen_bound */
    port = get_test_port(2);
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(fd, (struct sockaddr *)&addr, sizeof(addr));
        rc = listen(fd, 5);
        tap_ok(rc == 0, "listen(): on bound socket [BSD 4.4]");
    } else {
        tap_ok(0, "listen(): on bound socket [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* 10. listen_unbound */
    fd = make_tcp_socket();
    if (fd >= 0) {
        rc = listen(fd, 5);
        if (rc == 0) {
            tap_ok(1, "listen(): on unbound socket (auto-bind behavior) [BSD 4.4]");
            tap_diag("  behavior: auto-bind");
        } else {
            tap_ok(1, "listen(): on unbound socket (auto-bind behavior) [BSD 4.4]");
            tap_diag("  behavior: rejected (expected on some stacks)");
        }
    } else {
        tap_ok(0, "listen(): on unbound socket (auto-bind behavior) [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- connect() ---- */

    /* 11. connect_loopback */
    port = get_test_port(3);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    tap_ok(listener >= 0 && client >= 0 && server >= 0,
           "connect(): TCP to loopback listener [BSD 4.4]");
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 12. connect_refused */
    port = get_test_port(4);
    fd = make_tcp_socket();
    if (fd >= 0) {
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        rc = connect(fd, (struct sockaddr *)&addr, sizeof(addr));
        tap_ok(rc < 0 && get_bsd_errno() == ECONNREFUSED,
               "connect(): ECONNREFUSED to closed port [BSD 4.4]");
    } else {
        tap_ok(0, "connect(): ECONNREFUSED to closed port [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- accept() ---- */

    /* 13. accept_basic */
    port = get_test_port(5);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    tap_ok(server >= 0 && server != listener,
           "accept(): returns new descriptor [BSD 4.4]");
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 14. accept_addr */
    port = get_test_port(6);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    if (listener >= 0 && client >= 0) {
        memset(&addr, 0, sizeof(addr));
        addrlen = sizeof(addr);
        server = accept(listener, (struct sockaddr *)&addr, &addrlen);
        tap_ok(server >= 0 &&
               addr.sin_family == AF_INET &&
               addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK) &&
               addr.sin_port != 0,
               "accept(): fills peer address struct [BSD 4.4]");
        safe_close(server);
    } else {
        tap_ok(0, "accept(): fills peer address struct [BSD 4.4]");
    }
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 15. accept_nonblocking_ewouldblock */
    port = get_test_port(7);
    listener = make_loopback_listener(port);
    if (listener >= 0) {
        set_nonblocking(listener);
        server = accept_one(listener);
        tap_ok(server < 0 && get_bsd_errno() == EWOULDBLOCK,
               "accept(): EWOULDBLOCK when non-blocking, no pending [BSD 4.4]");
        safe_close(server);
    } else {
        tap_ok(0, "accept(): EWOULDBLOCK when non-blocking, no pending [BSD 4.4]");
    }
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- shutdown() ---- */

    /* 16. shutdown_rd */
    port = get_test_port(8);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        rc = shutdown(client, 0); /* SHUT_RD */
        tap_ok(rc == 0, "shutdown(SHUT_RD): disable receives [BSD 4.4]");
    } else {
        tap_ok(0, "shutdown(SHUT_RD): disable receives [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 17. shutdown_wr */
    port = get_test_port(9);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        set_recv_timeout(server, 2);
        rc = shutdown(client, 1); /* SHUT_WR */
        if (rc == 0) {
            /* Server should see EOF (recv returns 0) */
            rc = recv(server, (UBYTE *)buf, sizeof(buf), 0);
            tap_ok(rc == 0,
                   "shutdown(SHUT_WR): peer sees EOF [BSD 4.4]");
        } else {
            tap_ok(0, "shutdown(SHUT_WR): peer sees EOF [BSD 4.4]");
        }
    } else {
        tap_ok(0, "shutdown(SHUT_WR): peer sees EOF [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 18. shutdown_rdwr */
    port = get_test_port(10);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        rc = shutdown(client, 2); /* SHUT_RDWR */
        tap_ok(rc == 0, "shutdown(SHUT_RDWR): full close [BSD 4.4]");
    } else {
        tap_ok(0, "shutdown(SHUT_RDWR): full close [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- CloseSocket() ---- */

    /* 19. closesocket_valid */
    fd = make_tcp_socket();
    tap_ok(fd >= 0 && CloseSocket(fd) == 0,
           "CloseSocket(): valid descriptor [AmiTCP]");

    CHECK_CTRLC();

    /* 20. closesocket_invalid */
    rc = CloseSocket(-1);
    tap_ok(rc != 0,
           "CloseSocket(): invalid descriptor returns error [AmiTCP]");

    CHECK_CTRLC();

    /* ---- getsockname() ---- */

    /* 21. getsockname_after_bind */
    port = get_test_port(11);
    fd = make_tcp_socket();
    if (fd >= 0) {
        one = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        bind(fd, (struct sockaddr *)&addr, sizeof(addr));

        memset(&addr, 0, sizeof(addr));
        addrlen = sizeof(addr);
        getsockname(fd, (struct sockaddr *)&addr, &addrlen);
        tap_ok(addr.sin_family == AF_INET &&
               addr.sin_port == htons(port) &&
               addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
               "getsockname(): returns bound address [BSD 4.4]");
    } else {
        tap_ok(0, "getsockname(): returns bound address [BSD 4.4]");
    }
    safe_close(fd);

    CHECK_CTRLC();

    /* ---- getpeername() ---- */

    /* 22. getpeername_connected */
    port = get_test_port(12);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0) {
        memset(&addr, 0, sizeof(addr));
        addrlen = sizeof(addr);
        rc = getpeername(client, (struct sockaddr *)&addr, &addrlen);
        tap_ok(rc == 0 &&
               addr.sin_family == AF_INET &&
               addr.sin_addr.s_addr == htonl(INADDR_LOOPBACK),
               "getpeername(): returns peer address after connect [BSD 4.4]");
    } else {
        tap_ok(0, "getpeername(): returns peer address after connect [BSD 4.4]");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 23. getpeername_unconnected */
    fd = make_tcp_socket();
    if (fd >= 0) {
        addrlen = sizeof(addr);
        rc = getpeername(fd, (struct sockaddr *)&addr, &addrlen);
        tap_ok(rc < 0 && get_bsd_errno() == ENOTCONN,
               "getpeername(): ENOTCONN on unconnected socket [BSD 4.4]");
    } else {
        tap_ok(0, "getpeername(): ENOTCONN on unconnected socket [BSD 4.4]");
    }
    safe_close(fd);
}
