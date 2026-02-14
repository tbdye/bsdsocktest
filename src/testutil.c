/*
 * bsdsocktest — Shared test utilities
 */

#include "testutil.h"
#include "tap.h"

#include <proto/exec.h>
#include <proto/bsdsocket.h>

#include <sys/socket.h>
#include <sys/filio.h>
#include <netinet/in.h>

#include <string.h>

/* ---- Library state ---- */

struct Library *SocketBase;

static LONG bsd_errno;
static LONG bsd_h_errno;
static int base_port = DEFAULT_BASE_PORT;

/* Version string cached after open */
static const char *bsdlib_version_str;

/* ---- Library management ---- */

int open_bsdsocket(void)
{
    LONG result;

    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (!SocketBase) {
        tap_diag("Could not open bsdsocket.library v4");
        return -1;
    }

    result = SocketBaseTags(
        SBTM_SETVAL(SBTC_ERRNOLONGPTR), (ULONG)&bsd_errno,
        SBTM_SETVAL(SBTC_HERRNOLONGPTR), (ULONG)&bsd_h_errno,
        TAG_DONE);

    if (result != 0) {
        tap_diag("Warning: SocketBaseTags errno registration failed");
    }

    /* Cache the version string */
    bsdlib_version_str = NULL;
    SocketBaseTags(
        SBTM_GETREF(SBTC_RELEASESTRPTR), (ULONG)&bsdlib_version_str,
        TAG_DONE);

    return 0;
}

void close_bsdsocket(void)
{
    if (SocketBase) {
        CloseLibrary(SocketBase);
        SocketBase = NULL;
    }
    bsdlib_version_str = NULL;
}

const char *get_bsdsocket_version(void)
{
    return bsdlib_version_str;
}

LONG get_bsd_errno(void)
{
    return bsd_errno;
}

LONG get_bsd_h_errno(void)
{
    return bsd_h_errno;
}

void restore_bsd_errno(void)
{
    /* Restore via both SetErrnoPtr (resets size) and SocketBaseTags
     * (tag path) to fully undo SetErrnoPtr(&byte, 1) etc. */
    SetErrnoPtr(&bsd_errno, sizeof(bsd_errno));
    SocketBaseTags(
        SBTM_SETVAL(SBTC_ERRNOLONGPTR), (ULONG)&bsd_errno,
        SBTM_SETVAL(SBTC_HERRNOLONGPTR), (ULONG)&bsd_h_errno,
        TAG_DONE);
}

/* ---- Socket helpers ---- */

LONG make_tcp_socket(void)
{
    return socket(AF_INET, SOCK_STREAM, 0);
}

LONG make_udp_socket(void)
{
    return socket(AF_INET, SOCK_DGRAM, 0);
}

LONG make_loopback_listener(int port)
{
    LONG fd;
    struct sockaddr_in addr;
    LONG one = 1;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        CloseSocket(fd);
        return -1;
    }

    if (listen(fd, 5) < 0) {
        CloseSocket(fd);
        return -1;
    }

    return fd;
}

LONG make_loopback_client(int port)
{
    LONG fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        CloseSocket(fd);
        return -1;
    }

    return fd;
}

LONG accept_one(LONG listener_fd)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);

    return accept(listener_fd, (struct sockaddr *)&addr, &addrlen);
}

int set_nonblocking(LONG fd)
{
    LONG one = 1;
    return IoctlSocket(fd, FIONBIO, (APTR)&one);
}

int set_recv_timeout(LONG fd, int seconds)
{
    struct timeval tv;

    tv.tv_secs = seconds;
    tv.tv_micro = 0;

    return setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
}

void safe_close(LONG fd)
{
    if (fd >= 0)
        CloseSocket(fd);
}

void close_all(LONG *fds, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (fds[i] >= 0) {
            CloseSocket(fds[i]);
            fds[i] = -1;
        }
    }
}

/* ---- Port allocation ---- */

void set_base_port(int port)
{
    base_port = port;
}

int get_test_port(int offset)
{
    return base_port + offset;
}

/* ---- Data patterns ---- */

void fill_test_pattern(unsigned char *buf, int len, unsigned int seed)
{
    int i;

    for (i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        buf[i] = (unsigned char)(seed >> 16);
    }
}

int verify_test_pattern(const unsigned char *buf, int len, unsigned int seed)
{
    int i;
    unsigned char expected;

    for (i = 0; i < len; i++) {
        seed = seed * 1103515245 + 12345;
        expected = (unsigned char)(seed >> 16);
        if (buf[i] != expected)
            return i + 1;
    }

    return 0;
}
