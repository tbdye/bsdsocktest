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

#include <devices/timer.h>
#include <proto/timer.h>

#include <string.h>

/* ---- Library state ---- */

struct Library *SocketBase;
struct Device *TimerBase;

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

void reset_socket_state(void)
{
    LONG i;
    int cleaned = 0;

    /* Close any leftover sockets from previous runs.
     * On a clean library open this is a no-op (all CloseSocket fail). */
    for (i = 0; i < 64; i++) {
        if (CloseSocket(i) == 0)
            cleaned++;
    }

    if (cleaned > 0)
        tap_diagf("  reset: closed %d leftover socket(s)", cleaned);
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

/* ---- Signal helpers ---- */

BYTE alloc_signal(void)
{
    return AllocSignal(-1);
}

void free_signal(BYTE sigbit)
{
    if (sigbit >= 0)
        FreeSignal(sigbit);
}

/* ---- High-resolution timing (timer.device) ---- */

static struct MsgPort *timer_port;
static struct timerequest *timer_req;

int timer_init(void)
{
    timer_port = CreateMsgPort();
    if (!timer_port) {
        tap_diag("Could not create timer message port");
        return -1;
    }

    timer_req = (struct timerequest *)CreateIORequest(timer_port,
                    sizeof(struct timerequest));
    if (!timer_req) {
        DeleteMsgPort(timer_port);
        timer_port = NULL;
        tap_diag("Could not create timer I/O request");
        return -1;
    }

    if (OpenDevice((STRPTR)TIMERNAME, UNIT_MICROHZ,
                   (struct IORequest *)timer_req, 0) != 0) {
        DeleteIORequest((struct IORequest *)timer_req);
        DeleteMsgPort(timer_port);
        timer_req = NULL;
        timer_port = NULL;
        tap_diag("Could not open timer.device");
        return -1;
    }

    TimerBase = timer_req->tr_node.io_Device;
    return 0;
}

void timer_cleanup(void)
{
    if (timer_req) {
        CloseDevice((struct IORequest *)timer_req);
        DeleteIORequest((struct IORequest *)timer_req);
        timer_req = NULL;
    }
    if (timer_port) {
        DeleteMsgPort(timer_port);
        timer_port = NULL;
    }
    TimerBase = NULL;
}

void timer_now(struct bst_timestamp *ts)
{
    struct timeval tv;

    GetSysTime(&tv);
    ts->ts_secs = tv.tv_secs;
    ts->ts_micro = tv.tv_micro;
}

ULONG timer_elapsed_us(const struct bst_timestamp *start,
                       const struct bst_timestamp *end)
{
    ULONG secs;
    LONG micro;

    secs = end->ts_secs - start->ts_secs;
    micro = (LONG)end->ts_micro - (LONG)start->ts_micro;

    if (micro < 0) {
        secs--;
        micro += 1000000;
    }

    return secs * 1000000UL + (ULONG)micro;
}

ULONG timer_elapsed_ms(const struct bst_timestamp *start,
                       const struct bst_timestamp *end)
{
    ULONG us = timer_elapsed_us(start, end);

    return us / 1000 + ((us % 1000) >= 500 ? 1 : 0);
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
