/*
 * bsdsocktest — Host helper protocol implementation
 *
 * Manages the control channel connection to bsdsocktest_helper.py.
 * Line-based text protocol over TCP.
 */

#include "helper_proto.h"
#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>

#include <netinet/in.h>
#include <netdb.h>
#include <string.h>

/* Internal state */
static LONG ctrl_fd = -1;
static struct sockaddr_in resolved_addr;
static int connected = 0;

/* Read a line from the control socket.
 * Strips trailing \r and \n.
 * Returns length on success, 0 on EOF, -1 on error. */
static int recv_line(LONG fd, char *buf, int buflen)
{
    int pos = 0;
    char ch;
    LONG n;

    while (pos < buflen - 1) {
        n = recv(fd, &ch, 1, 0);
        if (n <= 0)
            return (n == 0 && pos > 0) ? pos : (int)n;
        if (ch == '\n')
            break;
        if (ch != '\r')
            buf[pos++] = ch;
    }
    buf[pos] = '\0';
    return pos;
}

int helper_connect(const char *host)
{
    LONG fd;
    ULONG ip;
    struct hostent *he;
    char line[64];
    int rc;

    if (connected)
        helper_quit();

    /* Resolve host — try inet_addr first, then gethostbyname */
    ip = inet_addr((STRPTR)host);
    if (ip == 0xFFFFFFFF) {
        he = gethostbyname((STRPTR)host);
        if (!he) {
            tap_diagf("  helper_connect: cannot resolve \"%s\"", host);
            return 0;
        }
        memcpy(&ip, he->h_addr_list[0], sizeof(ip));
    }

    /* Store resolved address */
    memset(&resolved_addr, 0, sizeof(resolved_addr));
    resolved_addr.sin_family = AF_INET;
    resolved_addr.sin_port = htons(HELPER_CTRL_PORT);
    resolved_addr.sin_addr.s_addr = ip;

    /* Connect to control channel */
    fd = make_tcp_socket();
    if (fd < 0) {
        tap_diag("  helper_connect: cannot create socket");
        return 0;
    }

    /* Workaround: UAE bsdsocket emulation processes socket() asynchronously.
     * A getsockopt round-trip ensures the fd is fully registered before
     * connect() attempts to use it.  Without this, connect() intermittently
     * returns EBADF on the just-created fd. */
    {
        LONG optval;
        LONG optlen = sizeof(optval);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wincompatible-pointer-types"
        getsockopt(fd, SOL_SOCKET, SO_TYPE, &optval, &optlen);
#pragma GCC diagnostic pop
    }

    if (connect(fd, (struct sockaddr *)&resolved_addr,
                sizeof(resolved_addr)) < 0) {
        tap_diagf("  helper_connect: connect failed, errno=%ld",
                  (long)get_bsd_errno());
        safe_close(fd);
        return 0;
    }

    /* Set recv timeout for protocol reads */
    set_recv_timeout(fd, 5);

    /* Read OK response */
    rc = recv_line(fd, line, sizeof(line));
    if (rc <= 0 || strcmp(line, "OK") != 0) {
        tap_diagf("  helper_connect: expected OK, got \"%s\" (rc=%d, errno=%ld)",
                  line, rc, (long)get_bsd_errno());
        safe_close(fd);
        return 0;
    }

    ctrl_fd = fd;
    connected = 1;
    return 1;
}

int helper_is_connected(void)
{
    return connected;
}

unsigned long helper_addr(void)
{
    return resolved_addr.sin_addr.s_addr;
}

long helper_connect_service(int port)
{
    LONG fd;
    struct sockaddr_in svc_addr;

    if (!connected)
        return -1;

    fd = make_tcp_socket();
    if (fd < 0)
        return -1;

    memcpy(&svc_addr, &resolved_addr, sizeof(svc_addr));
    svc_addr.sin_port = htons(port);

    if (connect(fd, (struct sockaddr *)&svc_addr, sizeof(svc_addr)) < 0) {
        tap_diagf("  helper_connect_service(%d): errno=%ld",
                  port, (long)get_bsd_errno());
        safe_close(fd);
        return -1;
    }

    return fd;
}

int helper_request_connect(int amiga_port)
{
    char cmd[32];
    char line[64];
    int len, rc;

    if (!connected)
        return 0;

    len = sprintf(cmd, "CONNECT %d\n", amiga_port);
    if (send(ctrl_fd, cmd, len, 0) != len)
        return 0;

    rc = recv_line(ctrl_fd, line, sizeof(line));
    if (rc <= 0)
        return 0;

    return (strcmp(line, "GO") == 0);
}

void helper_quit(void)
{
    if (connected) {
        /* Fire-and-forget — ignore send failure */
        send(ctrl_fd, "QUIT\n", 5, 0);
        safe_close(ctrl_fd);
    }
    ctrl_fd = -1;
    connected = 0;
}
