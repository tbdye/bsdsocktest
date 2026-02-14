/*
 * bsdsocktest — Shared test utilities
 *
 * Library management, socket helpers, port allocation, data patterns.
 */

#ifndef BSDSOCKTEST_TESTUTIL_H
#define BSDSOCKTEST_TESTUTIL_H

#include <exec/types.h>

/* Default base port for test sockets */
#define DEFAULT_BASE_PORT 7700

/* Not defined in Amiga netinet/in.h */
#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001UL
#endif

/* ---- Library management ---- */

/* Open bsdsocket.library v4+, register errno/h_errno pointers.
 * Returns 0 on success, -1 on failure (diagnostic emitted). */
int open_bsdsocket(void);

/* Close bsdsocket.library. */
void close_bsdsocket(void);

/* Get the bsdsocket.library version string (via SBTC_RELEASESTRPTR).
 * Returns NULL if library is not open or string is unavailable. */
const char *get_bsdsocket_version(void);

/* Access the bsdsocket errno value (separate from libnix errno). */
LONG get_bsd_errno(void);

/* Access the bsdsocket h_errno value. */
LONG get_bsd_h_errno(void);

/* ---- Socket helpers ---- */

/* Create a TCP (SOCK_STREAM) socket. Returns fd or -1. */
LONG make_tcp_socket(void);

/* Create a UDP (SOCK_DGRAM) socket. Returns fd or -1. */
LONG make_udp_socket(void);

/* Create a TCP listener on loopback at the given port.
 * Sets SO_REUSEADDR, binds, and calls listen(5).
 * Returns the listener fd or -1. */
LONG make_loopback_listener(int port);

/* Connect a TCP socket to loopback at the given port.
 * Returns the connected fd or -1. */
LONG make_loopback_client(int port);

/* Accept one connection on a listener socket.
 * Returns the accepted fd or -1. */
LONG accept_one(LONG listener_fd);

/* Set a socket to non-blocking mode via IoctlSocket(FIONBIO).
 * Returns 0 on success, -1 on failure. */
int set_nonblocking(LONG fd);

/* Set a receive timeout on a socket (in seconds).
 * Uses struct timeval with tv_secs/tv_micro.
 * Returns 0 on success, -1 on failure. */
int set_recv_timeout(LONG fd, int seconds);

/* Close a socket safely (ignores fd == -1). */
void safe_close(LONG fd);

/* Close an array of sockets. Sets each entry to -1 after closing. */
void close_all(LONG *fds, int count);

/* ---- Port allocation ---- */

/* Set the base port (from ReadArgs PORT/N parameter). */
void set_base_port(int port);

/* Get a test port: base + offset. */
int get_test_port(int offset);

/* ---- Data patterns ---- */

/* Fill a buffer with a deterministic test pattern seeded by 'seed'. */
void fill_test_pattern(unsigned char *buf, int len, unsigned int seed);

/* Verify a buffer matches the test pattern for the given seed.
 * Returns 0 if the pattern matches, or the 1-based byte offset
 * of the first mismatch. */
int verify_test_pattern(const unsigned char *buf, int len, unsigned int seed);

#endif /* BSDSOCKTEST_TESTUTIL_H */
