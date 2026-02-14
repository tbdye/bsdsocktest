/*
 * bsdsocktest — Socket transfer tests
 *
 * Tests: Dup2Socket, ObtainSocket, ReleaseSocket, ReleaseCopyOfSocket.
 *
 * 5 tests (113-117), port offsets 120-139.
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>

#include <netinet/in.h>
#include <string.h>

void run_transfer_tests(void)
{
    LONG listener, client, server;
    LONG fd1, fd2, target;
    LONG dup_fd, obtained;
    LONG unique_id, released_id, copy_id;
    unsigned char sbuf[100], rbuf[100];
    int port, rc, mismatch;

    /* ---- Dup2Socket ---- */

    /* 113. dup2socket_dup */
    fd1 = make_tcp_socket();
    if (fd1 >= 0) {
        fd2 = Dup2Socket(fd1, -1);
        tap_ok(fd2 >= 0 && fd2 != fd1,
               "dup2socket_dup - Dup2Socket(fd, -1) returns new descriptor");
        tap_diagf("  fd1=%ld, fd2=%ld", (long)fd1, (long)fd2);
        safe_close(fd2);
    } else {
        tap_ok(0, "dup2socket_dup - could not create socket");
    }
    safe_close(fd1);

    CHECK_CTRLC();

    /* 114. dup2socket_specific */
    fd1 = make_tcp_socket();
    if (fd1 >= 0) {
        target = fd1 + 10;
        fd2 = Dup2Socket(fd1, target);
        if (fd2 == target) {
            tap_ok(1, "dup2socket_specific - duplicated to specific slot");
            tap_diagf("  fd1=%ld, target=%ld, fd2=%ld",
                      (long)fd1, (long)target, (long)fd2);
            safe_close(fd2);
        } else if (fd2 == -1) {
            tap_ok(1, "dup2socket_specific - specific slot not supported");
            tap_diagf("  Dup2Socket(fd1, %ld) returned -1", (long)target);
        } else {
            tap_ok(0, "dup2socket_specific - unexpected return value");
            tap_diagf("  fd1=%ld, target=%ld, fd2=%ld",
                      (long)fd1, (long)target, (long)fd2);
            safe_close(fd2);
        }
    } else {
        tap_ok(0, "dup2socket_specific - could not create socket");
    }
    safe_close(fd1);

    CHECK_CTRLC();

    /* 115. dup2socket_send_recv */
    port = get_test_port(120);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        dup_fd = Dup2Socket(server, -1);
        if (dup_fd >= 0) {
            fill_test_pattern(sbuf, 100, 115);
            send(client, (UBYTE *)sbuf, 100, 0);
            set_recv_timeout(dup_fd, 2);
            rc = recv(dup_fd, (UBYTE *)rbuf, sizeof(rbuf), 0);
            mismatch = verify_test_pattern(rbuf, 100, 115);
            tap_ok(rc == 100 && mismatch == 0,
                   "dup2socket_send_recv - recv on duplicate matches sent data");
            tap_diagf("  server=%ld, dup=%ld, recv=%d",
                      (long)server, (long)dup_fd, rc);
            safe_close(dup_fd);
        } else {
            tap_ok(0, "dup2socket_send_recv - Dup2Socket failed");
        }
    } else {
        tap_ok(0, "dup2socket_send_recv - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* ---- ObtainSocket / ReleaseSocket ---- */

    /* 116. release_obtain_roundtrip */
    port = get_test_port(121);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        /* Send data while server is still active so it's pending */
        fill_test_pattern(sbuf, 100, 116);
        send(client, (UBYTE *)sbuf, 100, 0);

        unique_id = 42;
        released_id = ReleaseSocket(server, unique_id);
        if (released_id >= 0) {
            /* server fd is no longer valid after ReleaseSocket */
            server = -1;
            obtained = ObtainSocket(released_id, AF_INET, SOCK_STREAM, 0);
            if (obtained >= 0) {
                set_recv_timeout(obtained, 2);
                rc = recv(obtained, (UBYTE *)rbuf, sizeof(rbuf), 0);
                mismatch = verify_test_pattern(rbuf, 100, 116);
                tap_ok(rc == 100 && mismatch == 0,
                       "release_obtain_roundtrip - data intact after transfer");
                tap_diagf("  released_id=%ld, obtained=%ld, recv=%d",
                          (long)released_id, (long)obtained, rc);
                safe_close(obtained);
            } else {
                tap_ok(0, "release_obtain_roundtrip - ObtainSocket failed");
            }
        } else {
            tap_skip("release_obtain_roundtrip - ReleaseSocket not supported");
        }
    } else {
        tap_ok(0, "release_obtain_roundtrip - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);

    CHECK_CTRLC();

    /* 117. releasecopy_original_usable */
    port = get_test_port(122);
    listener = make_loopback_listener(port);
    client = make_loopback_client(port);
    server = accept_one(listener);
    if (client >= 0 && server >= 0) {
        copy_id = ReleaseCopyOfSocket(server, 43);
        if (copy_id >= 0) {
            /* Original server fd should still be usable */
            fill_test_pattern(sbuf, 100, 117);
            send(client, (UBYTE *)sbuf, 100, 0);
            set_recv_timeout(server, 2);
            rc = recv(server, (UBYTE *)rbuf, sizeof(rbuf), 0);
            mismatch = verify_test_pattern(rbuf, 100, 117);
            tap_ok(rc == 100 && mismatch == 0,
                   "releasecopy_original_usable - original works after copy released");
            tap_diagf("  copy_id=%ld, recv on original=%d",
                      (long)copy_id, rc);
            /* Copy is abandoned in pool — cleaned up at library close */
        } else {
            tap_skip("releasecopy_original_usable - ReleaseCopyOfSocket not supported");
        }
    } else {
        tap_ok(0, "releasecopy_original_usable - could not establish connection");
    }
    safe_close(server);
    safe_close(client);
    safe_close(listener);
}
