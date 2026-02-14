/*
 * bsdsocktest — Host helper protocol
 *
 * Communication with the Python host helper script.
 * Control channel protocol: line-based text (CONNECT/GO/QUIT).
 */

#ifndef HELPER_PROTO_H
#define HELPER_PROTO_H

/* Helper service ports — must match bsdsocktest_helper.py */
#define HELPER_CTRL_PORT    8700
#define HELPER_TCP_ECHO     8701
#define HELPER_UDP_ECHO     8702
#define HELPER_TCP_SINK     8703
#define HELPER_TCP_SOURCE   8704

/* Connect to helper's control channel.
 * host: IP address or hostname of the helper.
 * Returns 1 on success, 0 on failure. */
int helper_connect(const char *host);

/* Is the helper currently connected? */
int helper_is_connected(void);

/* Get the helper's resolved IP address (network byte order).
 * Only valid after successful helper_connect(). */
unsigned long helper_addr(void);

/* Connect to a helper TCP service port.
 * Returns socket fd on success, -1 on failure. */
long helper_connect_service(int port);

/* Request the helper to connect TO the Amiga on the specified port.
 * Uses the CONNECT protocol command.
 * Returns 1 if helper acknowledged (GO), 0 on failure. */
int helper_request_connect(int amiga_port);

/* Disconnect from helper. Safe to call if not connected. */
void helper_quit(void);

#endif /* HELPER_PROTO_H */
