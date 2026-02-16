# bsdsocktest --- Test Reference

## Introduction

This document is a test-by-test reference for **bsdsocktest**, an Amiga
bsdsocket.library conformance test suite. It covers all 142 tests organized
into 12 categories, with each entry documenting what the test validates, how
it works, and what a conforming implementation should do.

The test suite exercises the BSD socket API as exposed by Amiga TCP/IP stacks
(Roadshow, AmiTCP, Miami, Genesis) and by emulators (Amiberry, WinUAE). Tests
range from basic socket lifecycle operations through data transfer, socket
options, asynchronous I/O, name resolution, descriptor transfer, and
throughput benchmarks.

### How to Read This Document

Each test entry contains the following fields:

- **Category** --- The test category name as used by the `CATEGORY` ReadArgs
  parameter.
- **API** --- The primary bsdsocket.library or BSD function being tested.
- **Standard** --- The specification or standard that defines the expected
  behavior. Links point to the relevant section of the standard.
- **Rationale** --- Why this test exists and what conformance property it
  validates.
- **Methodology** --- What the test code does, step by step. Describes socket
  setup, operations performed, and the assertion that determines pass or fail.
- **Expected Result** --- What a conforming implementation returns or does.
Per-stack known failures and compatibility information are documented
separately in [COMPATIBILITY.md](COMPATIBILITY.md).

### Test Numbering

Test numbers are stable and deterministic. Every code path in every test
emits exactly one `tap_ok` or `tap_skip` call, so runtime test numbers are
fixed given the category execution order defined in `main.c`. The numbering
ranges are:

| Category   | Tests   | Count |
|------------|---------|-------|
| socket     | 1--23   | 23    |
| sendrecv   | 24--42  | 19    |
| sockopt    | 43--57  | 15    |
| waitselect | 58--72  | 15    |
| signals    | 73--87  | 15    |
| dns        | 88--104 | 17    |
| utility    | 105--114| 10    |
| transfer   | 115--119| 5     |
| errno      | 120--126| 7     |
| misc       | 127--131| 5     |
| icmp       | 132--136| 5     |
| throughput | 137--142| 6     |

### Standards Tags

Tests reference the following standards. The tag in square brackets at the end
of each test description string identifies the primary standard:

- **[BSD 4.4]** --- The BSD socket API as defined by 4.4BSD and documented in
  the FreeBSD man pages. Links use
  `https://man.freebsd.org/cgi/man.cgi?query=<func>&sektion=<N>`.
- **[POSIX]** --- IEEE Std 1003.1 (POSIX.1). Links use
  `https://pubs.opengroup.org/onlinepubs/9699919799/functions/<func>.html`.
- **[RFC 793]** --- Transmission Control Protocol. Defines TCP semantics
  including connection management, data transfer, and urgent data.
- **[RFC 768]** --- User Datagram Protocol. Defines UDP datagram semantics.
- **[RFC 792]** --- Internet Control Message Protocol. Defines ICMP echo
  (ping) and error messages.
- **[RFC 896]** / **[RFC 1122]** --- Nagle's algorithm and TCP host
  requirements.
- **[AmiTCP]** --- Amiga-specific extensions to the BSD socket API, including
  `CloseSocket()`, `WaitSelect()`, `IoctlSocket()`, `SocketBaseTags()`, and
  others. See [AMITCP_API.md](AMITCP_API.md) for full documentation.
- **[benchmark]** --- Performance measurement tests with no pass/fail
  standard. Results are reported as informational notes.

---

## Category: socket

Core socket lifecycle tests covering `socket()`, `bind()`, `listen()`,
`connect()`, `accept()`, `shutdown()`, `CloseSocket()`, `getsockname()`, and
`getpeername()`. All 23 tests in this category operate on loopback and require
no host helper.

### Test 1 --- socket(): create SOCK_STREAM (TCP)

**Category:** socket
**API:** socket()
**Standard:** [FreeBSD socket(2)](https://man.freebsd.org/cgi/man.cgi?query=socket&sektion=2)

**Rationale:** The ability to create a TCP socket is the most fundamental
operation in the BSD socket API. Every TCP/IP stack must support
`AF_INET` / `SOCK_STREAM`.

**Methodology:** Calls `socket(AF_INET, SOCK_STREAM, 0)` and checks that the
returned descriptor is non-negative. Closes the socket immediately after.

**Expected Result:** `socket()` returns a non-negative descriptor.

### Test 2 --- socket(): create SOCK_DGRAM (UDP)

**Category:** socket
**API:** socket()
**Standard:** [FreeBSD socket(2)](https://man.freebsd.org/cgi/man.cgi?query=socket&sektion=2)

**Rationale:** UDP sockets are the second fundamental socket type. A
conforming stack must support `AF_INET` / `SOCK_DGRAM`.

**Methodology:** Calls `socket(AF_INET, SOCK_DGRAM, 0)` and checks that the
returned descriptor is non-negative. Closes the socket immediately after.

**Expected Result:** `socket()` returns a non-negative descriptor.

### Test 3 --- socket(): create SOCK_RAW (ICMP)

**Category:** socket
**API:** socket()
**Standard:** [FreeBSD socket(2)](https://man.freebsd.org/cgi/man.cgi?query=socket&sektion=2)

**Rationale:** Raw sockets are required for ICMP operations (ping). Some
stacks restrict raw socket creation to privileged users.

**Methodology:** Calls `socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)`. If the
call succeeds (non-negative descriptor), the test passes. If it fails with
`EACCES`, the test is skipped with a diagnostic noting that raw sockets
require privileges. Any other failure is reported as a test failure.

**Expected Result:** `socket()` returns a non-negative descriptor, or fails
with `EACCES` if the stack restricts raw socket creation.

### Test 4 --- socket(): reject invalid domain (errno)

**Category:** socket
**API:** socket()
**Standard:** [FreeBSD socket(2)](https://man.freebsd.org/cgi/man.cgi?query=socket&sektion=2)

**Rationale:** A conforming implementation must reject invalid address family
values and set errno to indicate the error.

**Methodology:** Calls `socket(-1, SOCK_STREAM, 0)` with an invalid domain
value of -1. Checks that the call returns -1 and that `get_bsd_errno()`
returns a non-zero error code.

**Expected Result:** `socket()` returns -1 and sets errno to a non-zero value
(typically `EAFNOSUPPORT` or `EPROTONOSUPPORT`).

### Test 5 --- socket(): reject invalid type (errno)

**Category:** socket
**API:** socket()
**Standard:** [FreeBSD socket(2)](https://man.freebsd.org/cgi/man.cgi?query=socket&sektion=2)

**Rationale:** A conforming implementation must reject invalid socket type
values and set errno.

**Methodology:** Calls `socket(AF_INET, 999, 0)` with an invalid type value
of 999. Checks that the call returns -1 and that `get_bsd_errno()` returns a
non-zero error code.

**Expected Result:** `socket()` returns -1 and sets errno to a non-zero value
(typically `EPROTONOSUPPORT` or `ESOCKTNOSUPPORT`).

### Test 6 --- bind(): INADDR_ANY port 0 auto-assigns ephemeral port

**Category:** socket
**API:** bind()
**Standard:** [FreeBSD bind(2)](https://man.freebsd.org/cgi/man.cgi?query=bind&sektion=2)

**Rationale:** Binding to port 0 with `INADDR_ANY` must trigger automatic
ephemeral port assignment. This is essential for clients and for tests that
need unique ports without hardcoding.

**Methodology:** Creates a TCP socket, then calls `bind()` with `INADDR_ANY`
and port 0. After a successful bind, calls `getsockname()` to retrieve the
assigned address. Checks that `bind()` returned 0 and that the assigned port
(from `getsockname()`) is greater than 0.

**Expected Result:** `bind()` returns 0. `getsockname()` reports a port
number greater than 0.

### Test 7 --- bind(): specific port assignment

**Category:** socket
**API:** bind()
**Standard:** [FreeBSD bind(2)](https://man.freebsd.org/cgi/man.cgi?query=bind&sektion=2)

**Rationale:** Applications must be able to bind to a specific port number,
which is the normal mode for server sockets.

**Methodology:** Creates a TCP socket with `SO_REUSEADDR` set. Binds to
`INADDR_LOOPBACK` on a specific test port. After binding, calls
`getsockname()` to verify the assigned port matches the requested port.

**Expected Result:** `bind()` returns 0. `getsockname()` reports the exact
port number that was requested.

### Test 8 --- bind(): EADDRINUSE on double-bind

**Category:** socket
**API:** bind()
**Standard:** [FreeBSD bind(2)](https://man.freebsd.org/cgi/man.cgi?query=bind&sektion=2)

**Rationale:** Attempting to bind a second socket to an address that is
already in use by a listening socket must fail with `EADDRINUSE`. This
prevents accidental port conflicts.

**Methodology:** Creates two TCP sockets. Binds the first to
`INADDR_LOOPBACK` on a specific test port and calls `listen()`. Attempts to
bind the second socket to the same address and port. Checks that the second
`bind()` returns a negative value and that errno is `EADDRINUSE`.

**Expected Result:** The second `bind()` returns -1 with errno set to
`EADDRINUSE`.

### Test 9 --- listen(): on bound socket

**Category:** socket
**API:** listen()
**Standard:** [FreeBSD listen(2)](https://man.freebsd.org/cgi/man.cgi?query=listen&sektion=2)

**Rationale:** A socket that has been bound to an address must be able to
transition to the listening state via `listen()`.

**Methodology:** Creates a TCP socket with `SO_REUSEADDR`, binds it to
`INADDR_LOOPBACK` on a specific test port, and calls `listen()` with a
backlog of 5. Checks that `listen()` returns 0.

**Expected Result:** `listen()` returns 0.

### Test 10 --- listen(): on unbound socket (auto-bind behavior)

**Category:** socket
**API:** listen()
**Standard:** [FreeBSD listen(2)](https://man.freebsd.org/cgi/man.cgi?query=listen&sektion=2)

**Rationale:** The behavior of `listen()` on an unbound socket varies across
implementations. BSD 4.4 auto-binds to an ephemeral port; some stacks reject
the call. This test documents the behavior without mandating a specific
outcome.

**Methodology:** Creates a TCP socket without binding it and calls `listen()`
with a backlog of 5. The test always passes --- if `listen()` returns 0, a
diagnostic reports "auto-bind"; if it fails, a diagnostic reports "rejected".

**Expected Result:** The test passes regardless of outcome. The diagnostic
output records whether the stack auto-binds or rejects the call.

### Test 11 --- connect(): TCP to loopback listener

**Category:** socket
**API:** connect()
**Standard:** [FreeBSD connect(2)](https://man.freebsd.org/cgi/man.cgi?query=connect&sektion=2)

**Rationale:** Establishing a TCP connection to a loopback listener is the
standard client-server connection pattern. This validates the three-way
handshake on the loopback path.

**Methodology:** Creates a loopback listener on a test port, connects a
client socket to it, and accepts the connection on the listener. Checks that
all three descriptors (listener, client, server) are non-negative.

**Expected Result:** `connect()` succeeds and `accept()` returns a valid
server-side descriptor.

### Test 12 --- connect(): ECONNREFUSED to closed port

**Category:** socket
**API:** connect()
**Standard:** [FreeBSD connect(2)](https://man.freebsd.org/cgi/man.cgi?query=connect&sektion=2),
[RFC 793 Section 3.4](https://www.rfc-editor.org/rfc/rfc793#section-3.4)

**Rationale:** Connecting to a port where no listener exists must fail with
`ECONNREFUSED`. This validates that the stack correctly processes the RST
segment sent in response to the SYN.

**Methodology:** Creates a TCP socket and attempts to connect to a test port
on loopback where no listener has been established. Checks that `connect()`
returns a negative value and that errno is `ECONNREFUSED`.

**Expected Result:** `connect()` returns -1 with errno set to `ECONNREFUSED`.

### Test 13 --- accept(): returns new descriptor

**Category:** socket
**API:** accept()
**Standard:** [FreeBSD accept(2)](https://man.freebsd.org/cgi/man.cgi?query=accept&sektion=2)

**Rationale:** `accept()` must return a new, distinct descriptor for the
accepted connection --- it must not return the listener descriptor itself.

**Methodology:** Creates a loopback listener, connects a client, and calls
`accept()`. Checks that the returned descriptor is non-negative and is
different from the listener descriptor.

**Expected Result:** `accept()` returns a non-negative descriptor that is not
equal to the listener descriptor.

### Test 14 --- accept(): fills peer address struct

**Category:** socket
**API:** accept()
**Standard:** [FreeBSD accept(2)](https://man.freebsd.org/cgi/man.cgi?query=accept&sektion=2)

**Rationale:** When `accept()` is called with a non-NULL address argument, it
must fill in the connecting peer's address. Applications use this to identify
the remote end.

**Methodology:** Creates a loopback listener and connects a client. Calls
`accept()` with a `sockaddr_in` buffer. Checks that the returned descriptor
is non-negative, that the address family is `AF_INET`, that the address is
`INADDR_LOOPBACK`, and that the port is non-zero.

**Expected Result:** `accept()` returns a valid descriptor and populates the
address structure with `AF_INET`, the loopback address, and a non-zero port.

### Test 15 --- accept(): EWOULDBLOCK when non-blocking, no pending

**Category:** socket
**API:** accept()
**Standard:** [FreeBSD accept(2)](https://man.freebsd.org/cgi/man.cgi?query=accept&sektion=2)

**Rationale:** A non-blocking listener with no pending connections must return
immediately with `EWOULDBLOCK` rather than blocking the caller.

**Methodology:** Creates a loopback listener and sets it to non-blocking mode
via `IoctlSocket(FIONBIO)`. Calls `accept()` without any client having
connected. Checks that `accept()` returns a negative value and that errno is
`EWOULDBLOCK`.

**Expected Result:** `accept()` returns -1 with errno set to `EWOULDBLOCK`.

### Test 16 --- shutdown(SHUT_RD): disable receives

**Category:** socket
**API:** shutdown()
**Standard:** [FreeBSD shutdown(2)](https://man.freebsd.org/cgi/man.cgi?query=shutdown&sektion=2)

**Rationale:** `shutdown(SHUT_RD)` disables the receive side of a connection.
The call itself must succeed on a connected socket.

**Methodology:** Creates a TCP loopback connection pair (client and server).
Calls `shutdown(client, 0)` (SHUT_RD) on the client side. Checks that
`shutdown()` returns 0.

**Expected Result:** `shutdown()` returns 0.

### Test 17 --- shutdown(SHUT_WR): peer sees EOF

**Category:** socket
**API:** shutdown()
**Standard:** [FreeBSD shutdown(2)](https://man.freebsd.org/cgi/man.cgi?query=shutdown&sektion=2),
[RFC 793 Section 3.5](https://www.rfc-editor.org/rfc/rfc793#section-3.5)

**Rationale:** `shutdown(SHUT_WR)` sends a FIN to the peer, which must see
it as an end-of-file (recv returns 0). This is the standard mechanism for
half-closing a TCP connection.

**Methodology:** Creates a TCP loopback connection pair. Sets a 2-second
receive timeout on the server. Calls `shutdown(client, 1)` (SHUT_WR) on the
client. Calls `recv()` on the server and checks that it returns 0 (EOF).

**Expected Result:** `shutdown()` returns 0. The peer's `recv()` returns 0,
indicating EOF.

### Test 18 --- shutdown(SHUT_RDWR): full close

**Category:** socket
**API:** shutdown()
**Standard:** [FreeBSD shutdown(2)](https://man.freebsd.org/cgi/man.cgi?query=shutdown&sektion=2)

**Rationale:** `shutdown(SHUT_RDWR)` disables both directions of a
connection. The call must succeed on a connected socket.

**Methodology:** Creates a TCP loopback connection pair. Calls
`shutdown(client, 2)` (SHUT_RDWR) on the client. Checks that `shutdown()`
returns 0.

**Expected Result:** `shutdown()` returns 0.

### Test 19 --- CloseSocket(): valid descriptor

**Category:** socket
**API:** CloseSocket()
**Standard:** [AMITCP_API.md --- CloseSocket()](AMITCP_API.md#closesocket)

**Rationale:** `CloseSocket()` is the Amiga-specific replacement for BSD
`close()`. It must successfully close a valid socket descriptor.

**Methodology:** Creates a TCP socket and immediately calls `CloseSocket()`
on it. Checks that both the socket creation succeeded (non-negative
descriptor) and that `CloseSocket()` returned 0.

**Expected Result:** `CloseSocket()` returns 0.

### Test 20 --- CloseSocket(): invalid descriptor returns error

**Category:** socket
**API:** CloseSocket()
**Standard:** [AMITCP_API.md --- CloseSocket()](AMITCP_API.md#closesocket)

**Rationale:** Closing an invalid descriptor must return an error rather than
silently succeeding or crashing.

**Methodology:** Calls `CloseSocket(-1)` with an explicitly invalid
descriptor. Checks that the return value is non-zero (error).

**Expected Result:** `CloseSocket()` returns a non-zero error value.

### Test 21 --- getsockname(): returns bound address

**Category:** socket
**API:** getsockname()
**Standard:** [FreeBSD getsockname(2)](https://man.freebsd.org/cgi/man.cgi?query=getsockname&sektion=2)

**Rationale:** After binding a socket, `getsockname()` must return the exact
address and port that the socket is bound to. Applications rely on this to
discover their own local address.

**Methodology:** Creates a TCP socket with `SO_REUSEADDR`, binds it to
`INADDR_LOOPBACK` on a specific test port. Clears the address structure and
calls `getsockname()`. Checks that the returned family is `AF_INET`, the port
matches the bound port, and the address is `INADDR_LOOPBACK`.

**Expected Result:** `getsockname()` populates the address structure with
`AF_INET`, the correct port, and `INADDR_LOOPBACK`.

### Test 22 --- getpeername(): returns peer address after connect

**Category:** socket
**API:** getpeername()
**Standard:** [FreeBSD getpeername(2)](https://man.freebsd.org/cgi/man.cgi?query=getpeername&sektion=2)

**Rationale:** After a successful connection, `getpeername()` must return the
address of the remote peer. This is essential for server applications that
need to identify connected clients.

**Methodology:** Creates a TCP loopback connection pair. Calls
`getpeername()` on the client socket. Checks that the return value is 0, the
address family is `AF_INET`, and the address is `INADDR_LOOPBACK`.

**Expected Result:** `getpeername()` returns 0 and populates the address
structure with `AF_INET` and `INADDR_LOOPBACK`.

### Test 23 --- getpeername(): ENOTCONN on unconnected socket

**Category:** socket
**API:** getpeername()
**Standard:** [FreeBSD getpeername(2)](https://man.freebsd.org/cgi/man.cgi?query=getpeername&sektion=2)

**Rationale:** Calling `getpeername()` on a socket that is not connected must
fail with `ENOTCONN`. This is a standard error-path validation.

**Methodology:** Creates a TCP socket without connecting it. Calls
`getpeername()` and checks that it returns a negative value with errno set to
`ENOTCONN`.

**Expected Result:** `getpeername()` returns -1 with errno set to `ENOTCONN`.

---

## Category: sendrecv

Data transfer tests covering `send()`, `recv()`, `sendto()`, `recvfrom()`,
`sendmsg()`, `recvmsg()`, and associated flags and edge cases. Tests 24--38
operate on loopback. Tests 39--42 require the host helper for network-path
validation and are skipped when no helper is connected.

### Test 24 --- send()/recv(): 100-byte TCP transfer

**Category:** sendrecv
**API:** send(), recv()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2),
[FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** The most basic data transfer test. A conforming stack must be
able to send and receive a small TCP payload with data integrity.

**Methodology:** Creates a TCP loopback connection pair. Fills a 100-byte
buffer with a deterministic test pattern (seed 1). Sends the buffer from the
client. Calls `recv()` on the server with a 2-second timeout. Verifies that
exactly 100 bytes were received and that the test pattern matches.

**Expected Result:** `recv()` returns 100. The received data matches the
sent pattern with zero mismatches.

### Test 25 --- send()/recv(): 8192-byte TCP transfer (multi-recv)

**Category:** sendrecv
**API:** send(), recv()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2),
[FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** TCP is a stream protocol --- a single `send()` may be
delivered across multiple `recv()` calls. This test validates that the stack
correctly handles reassembly of a larger transfer.

**Methodology:** Creates a TCP loopback connection pair. Fills an 8192-byte
buffer with a deterministic test pattern (seed 2). Sends the entire buffer
from the client. Receives in a loop on the server (with a 3-second timeout)
until all 8192 bytes are accumulated or an error occurs. Verifies the total
byte count and pattern integrity.

**Expected Result:** The total received equals 8192 bytes. The received data
matches the sent pattern with zero mismatches.

### Test 26 --- recv(MSG_PEEK): read without consuming

**Category:** sendrecv
**API:** recv()
**Standard:** [FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** The `MSG_PEEK` flag allows an application to inspect incoming
data without removing it from the receive queue. This is used for protocol
detection and framing.

**Methodology:** Creates a TCP loopback connection pair. Sends 50 bytes with
a test pattern (seed 3) from the client. On the server, calls `recv()` with
`MSG_PEEK` and verifies 50 bytes are returned with the correct pattern. Then
clears the receive buffer and calls `recv()` again without `MSG_PEEK`. Checks
that the same 50 bytes are delivered again with the correct pattern ---
confirming that the peek did not consume the data.

**Expected Result:** Both the peek and the subsequent normal `recv()` return
50 bytes. The data matches the test pattern both times.

### Test 27 --- recv(MSG_OOB): urgent data delivery

**Category:** sendrecv
**API:** send(), recv()
**Standard:** [RFC 793 Section 3.7](https://www.rfc-editor.org/rfc/rfc793#section-3.7),
[FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** TCP urgent data (out-of-band) is defined in RFC 793. A
conforming implementation must deliver urgent bytes to receivers that request
them via the `MSG_OOB` flag.

**Methodology:** Creates a TCP loopback connection pair. Sends one byte
(0xAB) from the client with `MSG_OOB`. If the send itself fails, the test is
skipped. Otherwise, calls `recv()` with `MSG_OOB` on the server (with a
2-second timeout). Checks that exactly 1 byte is received and that its value
is 0xAB.

**Expected Result:** `recv(MSG_OOB)` returns 1, and the received byte is
0xAB.

### Test 28 --- sendto()/recvfrom(): UDP datagram loopback

**Category:** sendrecv
**API:** sendto(), recvfrom()
**Standard:** [RFC 768](https://www.rfc-editor.org/rfc/rfc768),
[FreeBSD sendto(2)](https://man.freebsd.org/cgi/man.cgi?query=sendto&sektion=2),
[FreeBSD recvfrom(2)](https://man.freebsd.org/cgi/man.cgi?query=recvfrom&sektion=2)

**Rationale:** UDP datagram transfer is the fundamental operation for
connectionless protocols. The test validates send, receive, data integrity,
and source address reporting on the loopback path.

**Methodology:** Creates two UDP sockets. Binds the receiver to
`INADDR_LOOPBACK` on a test port with `SO_REUSEADDR`. Fills a 100-byte
buffer with a test pattern (seed 3) and sends it via `sendto()` to the
receiver's address. The receiver calls `recvfrom()` with a 2-second timeout.
Checks that 100 bytes were received, the pattern matches, and the source
address is `INADDR_LOOPBACK`.

**Expected Result:** `recvfrom()` returns 100 bytes with correct data. The
source address is `INADDR_LOOPBACK`.

### Test 29 --- sendto(): correct dispatch after prior socket ops

**Category:** sendrecv
**API:** sendto()
**Standard:** [FreeBSD sendto(2)](https://man.freebsd.org/cgi/man.cgi?query=sendto&sektion=2)

**Rationale:** This test exercises the socket descriptor allocator to detect
a class of implementation bug where `sendto()` checks a stale socket
descriptor instead of the descriptor passed as an argument. Such a bug
causes sends to go to the wrong socket if a prior socket has been created
and closed.

**Methodology:** Creates a TCP socket and immediately closes it to exercise
the file descriptor allocator. Then creates two UDP sockets, binds the
receiver to `INADDR_LOOPBACK` on a test port, and sends 100 bytes from the
sender via `sendto()`. The receiver calls `recvfrom()` with a 2-second
timeout. Checks that 100 bytes were received and the pattern matches.

**Expected Result:** `recvfrom()` returns 100 bytes with correct data,
confirming that `sendto()` used the correct socket descriptor despite prior
allocator activity.

### Test 30 --- sendto(): on independently created socket

**Category:** sendrecv
**API:** sendto()
**Standard:** [FreeBSD sendto(2)](https://man.freebsd.org/cgi/man.cgi?query=sendto&sektion=2)

**Rationale:** Validates that a freshly created UDP socket can send datagrams
without any prior socket operations influencing the result. This is a
companion to test 29, verifying the baseline case.

**Methodology:** Creates two UDP sockets. Binds the receiver to
`INADDR_LOOPBACK` on a test port with `SO_REUSEADDR`. Fills a 100-byte
buffer with a test pattern (seed 5) and sends it via `sendto()`. The
receiver calls `recvfrom()` with a 2-second timeout. Checks that 100 bytes
were received and the pattern matches.

**Expected Result:** `recvfrom()` returns 100 bytes with correct data.

### Test 31 --- sendmsg()/recvmsg(): single iovec

**Category:** sendrecv
**API:** sendmsg(), recvmsg()
**Standard:** [FreeBSD sendmsg(2)](https://man.freebsd.org/cgi/man.cgi?query=sendmsg&sektion=2),
[FreeBSD recvmsg(2)](https://man.freebsd.org/cgi/man.cgi?query=recvmsg&sektion=2)

**Rationale:** `sendmsg()` and `recvmsg()` are the most general data
transfer functions, using `struct msghdr` and I/O vectors. This test
validates the basic case with a single iovec.

**Methodology:** Creates a TCP loopback connection pair. Fills a 100-byte
buffer with a test pattern (seed 6). Constructs a `msghdr` with one iovec
pointing to the buffer and sends via `sendmsg()`. On the server, constructs
a `msghdr` with one iovec and calls `recvmsg()` with a 2-second timeout.
Checks that 100 bytes were transferred and the pattern matches.

**Expected Result:** `sendmsg()` returns 100. `recvmsg()` returns 100. The
data matches the sent pattern.

### Test 32 --- sendmsg()/recvmsg(): scatter-gather (multiple iovecs)

**Category:** sendrecv
**API:** sendmsg(), recvmsg()
**Standard:** [FreeBSD sendmsg(2)](https://man.freebsd.org/cgi/man.cgi?query=sendmsg&sektion=2),
[FreeBSD recvmsg(2)](https://man.freebsd.org/cgi/man.cgi?query=recvmsg&sektion=2)

**Rationale:** Scatter-gather I/O using multiple iovecs is the primary
advantage of `sendmsg()`/`recvmsg()` over `send()`/`recv()`. A conforming
implementation must gather data from multiple buffers on send and scatter
received data into multiple buffers on receive.

**Methodology:** Creates a TCP loopback connection pair. Fills a 100-byte
buffer with a test pattern (seed 7). Constructs a `msghdr` with three
iovecs (50 + 30 + 20 = 100 bytes) and sends via `sendmsg()`. On the server,
constructs a `msghdr` with three iovecs of the same sizes and calls
`recvmsg()` with a 2-second timeout. Checks that 100 bytes were transferred
and the reassembled pattern matches.

**Expected Result:** `sendmsg()` returns 100. `recvmsg()` returns 100. The
data across all three receive iovecs matches the sent pattern.

### Test 33 --- recv(): EWOULDBLOCK on empty non-blocking socket

**Category:** sendrecv
**API:** recv()
**Standard:** [FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** A non-blocking socket with no data available must return
immediately with `EWOULDBLOCK` (or `EAGAIN`) rather than blocking. This is
essential for event-driven architectures.

**Methodology:** Creates a TCP loopback connection pair. Sets the server
socket to non-blocking mode via `IoctlSocket(FIONBIO)`. Calls `recv()` on
the server without any data having been sent. Checks that `recv()` returns a
negative value and that errno is `EWOULDBLOCK` or `EAGAIN`.

**Expected Result:** `recv()` returns -1 with errno set to `EWOULDBLOCK` or
`EAGAIN`.

### Test 34 --- send(): EWOULDBLOCK when buffer full

**Category:** sendrecv
**API:** send()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2)

**Rationale:** When the send buffer is full, a non-blocking `send()` must
return `EWOULDBLOCK` (or `EAGAIN`) rather than blocking. This validates
back-pressure signaling.

**Methodology:** Creates a TCP loopback connection pair. Sets the client to
non-blocking mode. Fills an 8192-byte buffer with a test pattern and sends
repeatedly in a loop (capped at 1 MB total) until `send()` returns a negative
value. Checks that the error is `EWOULDBLOCK` or `EAGAIN`. If the send buffer
never fills (more than 1 MB accepted), the test is skipped.

**Expected Result:** `send()` eventually returns -1 with errno set to
`EWOULDBLOCK` or `EAGAIN` after the buffer fills.

### Test 35 --- send(): error after peer closes connection

**Category:** sendrecv
**API:** send()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2),
[RFC 793 Section 3.4](https://www.rfc-editor.org/rfc/rfc793#section-3.4)

**Rationale:** After the peer closes a connection, the local stack should
deliver an error (typically `EPIPE` or `ECONNRESET`) when the application
attempts to send. This detects broken connections.

**Methodology:** Creates a TCP loopback connection pair. Closes the server
socket. Drains the FIN notification from the client with a 1-second receive
timeout. Then attempts up to 5 sends of 100 bytes each, interleaved with
1-second receive attempts, to give the stack every chance to deliver the
error. Checks whether any attempt returns an error with errno `EPIPE` or
`ECONNRESET`.

**Expected Result:** `send()` or `recv()` returns -1 with errno set to
`EPIPE` or `ECONNRESET` within 5 attempts.

### Test 36 --- send()/recv(): simultaneous bidirectional transfer

**Category:** sendrecv
**API:** send(), recv()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2),
[FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** TCP is a full-duplex protocol. Both sides of a connection must
be able to send and receive data simultaneously without interference.

**Methodology:** Creates a TCP loopback connection pair. Sends 200 bytes from
the client with test pattern seed 10, then sends 200 bytes from the server
with seed 11. Receives on the server side in a loop (with 2-second timeout)
and verifies the client's pattern. Receives on the client side in a loop and
verifies the server's pattern.

**Expected Result:** Both sides receive exactly 200 bytes with correct
pattern data.

### Test 37 --- recv(): behavior with zero-length buffer

**Category:** sendrecv
**API:** recv()
**Standard:** [FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** Calling `recv()` with a zero-length buffer is an edge case.
The key requirement is that it must not consume any data from the receive
queue --- subsequent receives must still see all pending data.

**Methodology:** Creates a TCP loopback connection pair. Sends 10 bytes from
the client with a test pattern (seed 12). Calls `recv()` on the server with
length 0 (the return value is logged as a diagnostic). Then calls `recv()`
normally with a 2-second timeout. Checks that all 10 bytes are received and
the pattern matches.

**Expected Result:** The normal `recv()` after the zero-length receive
returns 10 bytes with correct data, confirming no data was consumed.

### Test 38 --- send(): zero-length send

**Category:** sendrecv
**API:** send()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2)

**Rationale:** Sending zero bytes is an edge case. The key requirement is
that it must not corrupt the connection --- subsequent data transfers must
work normally.

**Methodology:** Creates a TCP loopback connection pair. Calls `send()` with
length 0 on the client (the return value is logged as a diagnostic). Then
sends 10 bytes with a test pattern (seed 13) and receives on the server with
a 2-second timeout. Checks that 10 bytes are received and the pattern
matches.

**Expected Result:** The connection remains functional after the zero-length
send. The subsequent 10-byte transfer succeeds with correct data.

### Test 39 --- send()/recv(): 64KB TCP integrity via network

**Category:** sendrecv
**API:** send(), recv()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2),
[FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** Network-path data integrity is fundamentally different from
loopback. Real network transfers traverse device drivers, bridge interfaces,
and potentially different MTUs. This test validates that 64 KB of data
survives a round trip through the host helper's TCP echo service.

**Methodology:** Connects to the host helper's TCP echo service. Fills an
8192-byte buffer with a test pattern (seed 0) and sends it 8 times (64 KB
total) with a 10-second receive timeout. Receives in a loop, verifying each
8192-byte chunk against the pattern as it arrives (incremental chunk
verification). Checks that at least 65536 verified bytes were received.
Skipped if no host helper is connected.

**Expected Result:** At least 65536 bytes received with correct pattern data.

### Test 40 --- sendto()/recvfrom(): UDP datagram via network

**Category:** sendrecv
**API:** sendto(), recvfrom()
**Standard:** [RFC 768](https://www.rfc-editor.org/rfc/rfc768),
[FreeBSD sendto(2)](https://man.freebsd.org/cgi/man.cgi?query=sendto&sektion=2),
[FreeBSD recvfrom(2)](https://man.freebsd.org/cgi/man.cgi?query=recvfrom&sektion=2)

**Rationale:** UDP datagram transfer over a real network validates the
complete path including fragmentation handling, checksum verification, and
address resolution. This complements the loopback UDP test (test 28).

**Methodology:** Creates a UDP socket. Fills a 512-byte buffer with a test
pattern (seed 0x55) and sends it via `sendto()` to the host helper's UDP
echo port. Calls `recvfrom()` with a 5-second timeout. Checks that 512 bytes
were received and the pattern matches. Skipped if no host helper is
connected.

**Expected Result:** `recvfrom()` returns 512 bytes with correct data.

### Test 41 --- accept(): incoming connection from remote host

**Category:** sendrecv
**API:** accept()
**Standard:** [FreeBSD accept(2)](https://man.freebsd.org/cgi/man.cgi?query=accept&sektion=2)

**Rationale:** Accepting an incoming connection from a remote host (as
opposed to loopback) exercises the network-facing accept path. This validates
that the stack can handle real TCP connections arriving from another machine.

**Methodology:** Creates a TCP listener bound to `INADDR_ANY` on a test port
with `SO_REUSEADDR`. Asks the host helper (via the control protocol) to
connect to the Amiga on that port. Uses `WaitSelect()` with a 5-second
timeout to wait for the incoming connection. Calls `accept()`, then receives
data and checks for the helper's greeting string ("BSDSOCKTEST HELLO FROM
HELPER\n", 30 bytes). Skipped if no host helper is connected.

**Expected Result:** `accept()` returns a valid descriptor. 30 bytes of the
expected greeting string are received.

### Test 42 --- send()/recv(): 256KB+ TCP integrity via network

**Category:** sendrecv
**API:** send(), recv()
**Standard:** [FreeBSD send(2)](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2),
[FreeBSD recv(2)](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2)

**Rationale:** A larger network transfer (256 KB) exercises TCP windowing,
congestion control, and sustained throughput. It also provides a baseline
throughput measurement via the host helper echo service.

**Methodology:** Connects to the host helper's TCP echo service. Fills an
8192-byte buffer with a test pattern (seed 0) and sends it 32 times (256 KB
total) with a 30-second receive timeout. Receives in a loop with incremental
8192-byte chunk verification. Records timestamps before and after the
transfer and computes throughput in KB/s. Checks that at least 262144
verified bytes were received. Reports throughput as a TAP note. Skipped if
no host helper is connected.

**Expected Result:** At least 262144 bytes received with correct pattern
data. Throughput is reported as an informational metric.

---

## Category: sockopt

Socket option tests covering `getsockopt()`, `setsockopt()`, and
`IoctlSocket()`. The tests validate query and modification of socket-level
options (`SO_TYPE`, `SO_REUSEADDR`, `SO_KEEPALIVE`, `SO_LINGER`,
`SO_RCVTIMEO`, `SO_SNDTIMEO`, `SO_ERROR`, `SO_RCVBUF`, `SO_SNDBUF`),
protocol-level options (`TCP_NODELAY`), and Amiga-specific I/O control
requests (`FIONBIO`, `FIONREAD`, `FIOASYNC`). All 15 tests operate on
loopback and require no host helper.

### Test 43 --- getsockopt(SO_TYPE): query socket type

**Category:** sockopt
**API:** getsockopt()
**Standard:** [FreeBSD getsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=getsockopt&sektion=2)

**Rationale:** `SO_TYPE` allows an application to query the type of an
existing socket descriptor. A conforming implementation must return
`SOCK_STREAM` for TCP sockets and `SOCK_DGRAM` for UDP sockets.

**Methodology:** Creates one TCP socket and one UDP socket. Calls
`getsockopt(SOL_SOCKET, SO_TYPE)` on the TCP socket and checks that the
returned value is `SOCK_STREAM`. Calls `getsockopt(SOL_SOCKET, SO_TYPE)` on
the UDP socket and checks that the returned value is `SOCK_DGRAM`. Both
conditions must hold for the test to pass.

**Expected Result:** `getsockopt(SO_TYPE)` returns `SOCK_STREAM` for the TCP
socket and `SOCK_DGRAM` for the UDP socket.

### Test 44 --- SO_REUSEADDR: query default value

**Category:** sockopt
**API:** getsockopt()
**Standard:** [FreeBSD getsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=getsockopt&sektion=2)

**Rationale:** The default value of `SO_REUSEADDR` on a freshly created
socket varies across implementations (typically 0). This test documents the
default without mandating a specific value.

**Methodology:** Creates a TCP socket. Calls `getsockopt(SOL_SOCKET,
SO_REUSEADDR)` to retrieve the default value. The test always passes --- the
default value is logged as a diagnostic.

**Expected Result:** The test passes regardless of the default value. The
diagnostic output records the default `SO_REUSEADDR` value.

### Test 45 --- SO_REUSEADDR: enable address reuse

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [FreeBSD setsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2)

**Rationale:** `SO_REUSEADDR` must be settable to a non-zero value, which
enables binding to an address that is already in a `TIME_WAIT` state. This
is critical for server applications that restart frequently.

**Methodology:** Creates a TCP socket. Calls `setsockopt(SOL_SOCKET,
SO_REUSEADDR)` with a value of 1. Then calls `getsockopt(SOL_SOCKET,
SO_REUSEADDR)` to read it back. Checks that `setsockopt()` returned 0 and
that the read-back value is non-zero.

**Expected Result:** `setsockopt()` returns 0. `getsockopt()` reports a
non-zero value for `SO_REUSEADDR`.

### Test 46 --- SO_REUSEADDR: clear and read-back behavior

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [FreeBSD setsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2)

**Rationale:** Applications need to be able to clear `SO_REUSEADDR` after
setting it. This test validates the set-to-zero and read-back cycle.

**Methodology:** Creates a TCP socket. Calls `setsockopt(SOL_SOCKET,
SO_REUSEADDR)` with a value of 0. Then calls `getsockopt(SOL_SOCKET,
SO_REUSEADDR)` to read it back. The test always passes --- if the read-back
value is still non-zero, a diagnostic notes that the option could not be
cleared (some implementations always return a non-zero value).

**Expected Result:** The test passes regardless of outcome. If
`SO_REUSEADDR` was successfully cleared, the read-back value is 0. If
clearing is not supported, the diagnostic records the limitation.

### Test 47 --- SO_KEEPALIVE: enable keepalive probes

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [RFC 1122 Section 4.2.3.6](https://www.rfc-editor.org/rfc/rfc1122#section-4.2.3.6)

**Rationale:** `SO_KEEPALIVE` enables periodic probing of idle connections to
detect dead peers. RFC 1122 specifies that TCP implementations should
support this option.

**Methodology:** Creates a TCP socket. Calls `setsockopt(SOL_SOCKET,
SO_KEEPALIVE)` with a value of 1. Then calls `getsockopt(SOL_SOCKET,
SO_KEEPALIVE)` to read it back. Checks that `setsockopt()` returned 0 and
that the read-back value is non-zero.

**Expected Result:** `setsockopt()` returns 0. `getsockopt()` reports a
non-zero value for `SO_KEEPALIVE`.

### Test 48 --- SO_LINGER: set and read back linger struct

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [FreeBSD setsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2)

**Rationale:** `SO_LINGER` controls whether `CloseSocket()` blocks to allow
pending data to drain. The option uses a `struct linger` with `l_onoff` and
`l_linger` fields. A conforming implementation must store and return both
fields correctly.

**Methodology:** Creates a TCP socket. Constructs a `struct linger` with
`l_onoff = 1` and `l_linger = 5` and calls `setsockopt(SOL_SOCKET,
SO_LINGER)`. Clears the structure and calls `getsockopt(SOL_SOCKET,
SO_LINGER)` to read it back. Checks that `setsockopt()` returned 0,
`l_onoff` is non-zero, and `l_linger` is 5.

**Expected Result:** `setsockopt()` returns 0. The read-back `l_onoff` is
non-zero and `l_linger` is 5.

### Test 49 --- SO_RCVTIMEO: set receive timeout

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [FreeBSD setsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2)

**Rationale:** `SO_RCVTIMEO` sets a timeout on receive operations. A conforming
stack must accept and store the timeout value. Actual enforcement (blocking
`recv()` returning `EWOULDBLOCK` after the timeout expires) cannot be safely
tested because stacks that accept the option but don't enforce it will hang
indefinitely on `recv()`. Enforcement has been validated on Roadshow.

**Methodology:** Creates a TCP socket. Sets `SO_RCVTIMEO` to 1 second via
`setsockopt()`. If the `setsockopt()` call fails, the test fails (the option
should be supported). Otherwise, clears the `timeval` structure, calls
`getsockopt()` to read back the value, and verifies the returned seconds and
microseconds match the set values.

**Expected Result:** `setsockopt()` returns 0. The read-back `tv_secs` is 1
and `tv_micro` is 0.

### Test 50 --- SO_SNDTIMEO: set send timeout

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [FreeBSD setsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2)

**Rationale:** `SO_SNDTIMEO` sets a timeout on send operations. A conforming
implementation must accept and store this timeout value correctly.

**Methodology:** Creates a TCP socket. Calls `setsockopt(SOL_SOCKET,
SO_SNDTIMEO)` with a `struct timeval` of 1 second. If the `setsockopt()`
call succeeds, calls `getsockopt(SOL_SOCKET, SO_SNDTIMEO)` to read it back.
Checks that the read-back value has `tv_secs == 1` and `tv_micro == 0`.

**Expected Result:** `setsockopt()` returns 0. `getsockopt()` reads back a
`struct timeval` with `tv_secs = 1` and `tv_micro = 0`.

### Test 51 --- TCP_NODELAY: disable Nagle algorithm

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [RFC 896](https://www.rfc-editor.org/rfc/rfc896),
[RFC 1122 Section 4.2.3.4](https://www.rfc-editor.org/rfc/rfc1122#section-4.2.3.4)

**Rationale:** `TCP_NODELAY` disables Nagle's algorithm, which coalesces
small segments. Disabling Nagle is required for latency-sensitive protocols
such as interactive terminal sessions and real-time protocols.

**Methodology:** Creates a TCP socket. Calls `setsockopt(IPPROTO_TCP,
TCP_NODELAY)` with a value of 1. Then calls `getsockopt(IPPROTO_TCP,
TCP_NODELAY)` to read it back. Checks that `setsockopt()` returned 0 and
that the read-back value is non-zero.

**Expected Result:** `setsockopt()` returns 0. `getsockopt()` reports a
non-zero value for `TCP_NODELAY`.

### Test 52 --- SO_ERROR: pending error after failed connect

**Category:** sockopt
**API:** getsockopt(), connect()
**Standard:** [FreeBSD getsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=getsockopt&sektion=2)

**Rationale:** `SO_ERROR` retrieves the pending error code stored on a
socket. After a non-blocking connect to a closed port fails, the error must
be retrievable via `getsockopt(SO_ERROR)`. This is the standard mechanism
for detecting asynchronous connection failures.

**Methodology:** Creates a TCP socket and sets it to non-blocking mode.
Initiates a `connect()` to a loopback port where no listener exists. If the
connect returns `EINPROGRESS`, waits for completion using `WaitSelect()` with
a 2-second timeout on the write set, then calls `getsockopt(SOL_SOCKET,
SO_ERROR)` and checks that the pending error is `ECONNREFUSED`. If the
non-blocking connect returns `ECONNREFUSED` immediately (some stacks resolve
loopback connections synchronously), the test passes and logs the immediate
error path.

**Expected Result:** `getsockopt(SO_ERROR)` returns `ECONNREFUSED` after the
non-blocking connect fails.

### Test 53 --- SO_RCVBUF: set receive buffer size

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [FreeBSD setsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2)

**Rationale:** `SO_RCVBUF` controls the size of the kernel receive buffer.
Applications that process data in bursts often need to increase this value
to prevent drops.

**Methodology:** Creates a TCP socket. Calls `setsockopt(SOL_SOCKET,
SO_RCVBUF)` with a value of 32768. Then calls `getsockopt(SOL_SOCKET,
SO_RCVBUF)` to read it back. Checks that `setsockopt()` returned 0 and that
the read-back value is at least 32768 (the kernel may round up).

**Expected Result:** `setsockopt()` returns 0. `getsockopt()` reports a
receive buffer size of at least 32768 bytes.

### Test 54 --- SO_SNDBUF: set send buffer size

**Category:** sockopt
**API:** setsockopt(), getsockopt()
**Standard:** [FreeBSD setsockopt(2)](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2)

**Rationale:** `SO_SNDBUF` controls the size of the kernel send buffer.
Larger send buffers improve throughput for bulk data transfers; smaller
buffers reduce latency.

**Methodology:** Creates a TCP socket. Calls `setsockopt(SOL_SOCKET,
SO_SNDBUF)` with a value of 32768. Then calls `getsockopt(SOL_SOCKET,
SO_SNDBUF)` to read it back. Checks that `setsockopt()` returned 0 and that
the read-back value is at least 32768 (the kernel may round up).

**Expected Result:** `setsockopt()` returns 0. `getsockopt()` reports a send
buffer size of at least 32768 bytes.

### Test 55 --- IoctlSocket(FIONBIO): set non-blocking mode

**Category:** sockopt
**API:** IoctlSocket()
**Standard:** [AMITCP_API.md --- IoctlSocket()](AMITCP_API.md#ioctlsocket)

**Rationale:** `IoctlSocket(FIONBIO)` is the Amiga-specific mechanism for
setting a socket to non-blocking mode. Non-blocking sockets are essential
for event-driven I/O and asynchronous connection handling.

**Methodology:** Creates a TCP socket. Calls `IoctlSocket(fd, FIONBIO)`
with a pointer to an integer value of 1. If the ioctl succeeds, attempts a
`connect()` to a loopback port with no listener. Checks that the connect
returns an immediate error of either `EINPROGRESS` (asynchronous failure
pending) or `ECONNREFUSED` (synchronous failure on loopback), confirming
that the socket is operating in non-blocking mode.

**Expected Result:** `IoctlSocket(FIONBIO)` returns 0. The subsequent
`connect()` returns -1 with errno `EINPROGRESS` or `ECONNREFUSED`.

### Test 56 --- IoctlSocket(FIONREAD): query pending bytes

**Category:** sockopt
**API:** IoctlSocket()
**Standard:** [AMITCP_API.md --- IoctlSocket()](AMITCP_API.md#ioctlsocket)

**Rationale:** `IoctlSocket(FIONREAD)` queries the number of bytes available
to be read from a socket without consuming them. This is used by protocols
that need to know the pending data size before allocating a receive buffer.

**Methodology:** Creates a TCP loopback connection pair. Sends 100 bytes
with a test pattern (seed 20) from the client. Uses `WaitSelect()` with a
1-second timeout to ensure the data has arrived at the server. Calls
`IoctlSocket(server, FIONREAD)` and checks that the return value is 0
(success) and the reported byte count is exactly 100.

**Expected Result:** `IoctlSocket(FIONREAD)` returns 0 and reports 100
bytes pending.

### Test 57 --- IoctlSocket(FIOASYNC): async notification mode

**Category:** sockopt
**API:** IoctlSocket()
**Standard:** [AMITCP_API.md --- IoctlSocket()](AMITCP_API.md#ioctlsocket)

**Rationale:** `FIOASYNC` enables asynchronous notification mode on a
socket. Not all Amiga TCP/IP stacks support this ioctl request; the test
documents the behavior and skips when it is not available.

**Methodology:** Creates a TCP socket. Calls `IoctlSocket(fd, FIOASYNC)`
with a pointer to an integer value of 1. If the call returns 0, the test
passes. If it returns an error, the test is skipped with a diagnostic
noting that `FIOASYNC` is not supported.

**Expected Result:** `IoctlSocket(FIOASYNC)` returns 0, or the test is
skipped if the stack does not support this request.

---

## Category: waitselect

Asynchronous I/O readiness tests covering `WaitSelect()`, the Amiga
replacement for BSD `select()`. These tests validate read and write
readiness detection, timeout behavior, NULL fd set handling, exception fd
notification, signal interruption, the nfds boundary convention, high
descriptor support, non-blocking connect completion, and peer close
detection. All 15 tests operate on loopback and require no host helper.

### Test 58 --- WaitSelect(): read readiness after data send

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** The most fundamental `WaitSelect()` test: a socket with
pending data must be reported as readable. This validates the basic
integration between the send path and the readiness notification mechanism.

**Methodology:** Creates a TCP loopback connection pair. Sends 100 bytes
with a test pattern (seed 70) from the client. Calls `WaitSelect()` with the
server socket in the read set and a 2-second timeout. Checks that the
return value is at least 1 and that the server socket is set in the returned
read set.

**Expected Result:** `WaitSelect()` returns a value >= 1 with the server
socket marked readable.

### Test 59 --- WaitSelect(): write readiness on connected socket

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** A connected socket with available send buffer space must be
reported as writable. This is the write-side counterpart to test 58.

**Methodology:** Creates a TCP loopback connection pair. Calls
`WaitSelect()` with the client socket in the write set and a 2-second
timeout. No data has been sent, so the send buffer is empty. Checks that
the return value is at least 1 and that the client socket is set in the
returned write set.

**Expected Result:** `WaitSelect()` returns a value >= 1 with the client
socket marked writable.

### Test 60 --- WaitSelect(): tv={0,0} immediate poll

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** A timeout of `{0, 0}` must cause `WaitSelect()` to poll once
and return immediately without blocking. This is the standard polling
pattern used by event loops.

**Methodology:** Creates a TCP loopback connection pair. Calls
`WaitSelect()` with the server in the read set and a timeout of `{0, 0}`.
Records timestamps before and after the call. Checks that the return value
is 0 (no descriptors ready, since no data was sent) and that the elapsed
time is less than 100ms.

**Expected Result:** `WaitSelect()` returns 0 (no ready descriptors). The
call completes in under 100ms.

### Test 61 --- WaitSelect(): timeout fires when idle

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** When no descriptors become ready within the specified timeout,
`WaitSelect()` must return 0 after approximately the timeout duration. This
validates the timer integration.

**Methodology:** Creates a TCP loopback connection pair. Calls
`WaitSelect()` with the server in the read set and a 1-second timeout. No
data is sent, so no descriptor becomes ready. Records timestamps before and
after the call. Checks that the return value is 0 and that the elapsed time
is between 500ms and 2000ms.

**Expected Result:** `WaitSelect()` returns 0 after approximately 1 second
(between 500ms and 2000ms).

### Test 62 --- WaitSelect(): NULL timeout blocks until activity

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** A NULL timeout means `WaitSelect()` must block indefinitely
until at least one descriptor becomes ready. This validates that the call
does not return prematurely when no timeout is set.

**Methodology:** Creates a loopback listener and connects a client. The
pending connection in the listener's backlog makes the listener "readable".
Calls `WaitSelect()` with the listener in the read set and a NULL timeout
pointer. Checks that the return value is at least 1 and that the listener
is set in the returned read set. Accepts the pending connection to clean up.

**Expected Result:** `WaitSelect()` returns a value >= 1 with the listener
socket marked readable.

### Test 63 --- WaitSelect(): all NULL fdsets + timeout = delay

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** Calling `WaitSelect()` with all three fd set pointers NULL
and a timeout is the standard portable delay mechanism in AmiTCP
applications. The call must sleep for approximately the requested duration
and return 0.

**Methodology:** Calls `WaitSelect(0, NULL, NULL, NULL, &tv, NULL)` with a
timeout of 250ms. Records timestamps before and after the call. Checks that
the return value is 0 and that the elapsed time is between 100ms and 600ms.

**Expected Result:** `WaitSelect()` returns 0 after approximately 250ms
(between 100ms and 600ms).

### Test 64 --- WaitSelect(): exceptfds detects OOB data

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect),
[RFC 793 Section 3.7](https://www.rfc-editor.org/rfc/rfc793#section-3.7)

**Rationale:** The exception fd set is the standard mechanism for detecting
TCP urgent (out-of-band) data. When a peer sends data with `MSG_OOB`, the
receiving socket must be flagged in the exception set.

**Methodology:** Creates a TCP loopback connection pair. Sends 1 byte with
`MSG_OOB` from the client. If the send fails, the test is skipped. Otherwise,
calls `WaitSelect()` with the server in the exception set and a 2-second
timeout. Checks that the return value is at least 1 and that the server
socket is set in the returned exception set.

**Expected Result:** `WaitSelect()` returns a value >= 1 with the server
socket marked in the exception set.

### Test 65 --- WaitSelect(): multiple sockets in readfds

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** `WaitSelect()` must correctly track readiness across multiple
sockets simultaneously. Applications like BBS servers monitor many
connections at once.

**Methodology:** Creates three independent TCP loopback connection pairs.
Sends 10 bytes from each client to its corresponding server. Builds a read
set containing all three server sockets. Calls `WaitSelect()` with a 2-second
timeout. Counts how many of the three servers are set in the returned read
set. Checks that the return value is at least 1 and that all three servers
are marked readable.

**Expected Result:** `WaitSelect()` returns a value >= 1 with all three
server sockets marked readable.

### Test 66 --- WaitSelect(): Amiga signal interruption

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** `WaitSelect()` extends BSD `select()` with an Amiga-specific
`sigmask` parameter that allows the caller to wake up on Exec signals in
addition to socket activity. When a signal fires, `WaitSelect()` must return
immediately with the signal reported in the output mask.

**Methodology:** Allocates a signal bit. Creates a loopback listener. Sends
the signal to the current task via `Signal(FindTask(NULL), ...)` before
calling `WaitSelect()`. Calls `WaitSelect()` with the listener in the read
set, a NULL timeout, and the signal mask. Checks that the return value is 0
(no socket activity), the listener is not in the read set, and the signal
bit is set in the returned signal mask.

**Expected Result:** `WaitSelect()` returns 0. The listener is not marked
readable. The signal mask reports the allocated signal bit.

### Test 67 --- WaitSelect(): signal mask passthrough

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** When `WaitSelect()` returns due to socket readiness (not a
signal), the behavior of the signal mask output varies across
implementations. This test documents whether the mask is cleared or
preserved when socket activity triggers the return.

**Methodology:** Allocates a signal bit. Creates a TCP loopback connection
pair. Sends 100 bytes with a test pattern (seed 79) from the client. Calls
`WaitSelect()` with the server in the read set, a 2-second timeout, and the
signal mask. No signal is sent. Checks that the return value is at least 1
and that the server is marked readable. The resulting signal mask value is
logged as a diagnostic.

**Expected Result:** `WaitSelect()` returns a value >= 1 with the server
socket marked readable. The signal mask behavior is documented as a
diagnostic.

### Test 68 --- WaitSelect(): invalid descriptor handling

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** Passing a closed or invalid descriptor in an fd set is a
programming error. The test documents how the stack handles this case ---
whether it returns `EBADF` or some other behavior.

**Methodology:** Creates a TCP socket, records its descriptor number, and
immediately closes it. Builds a read set containing the now-invalid
descriptor. Calls `WaitSelect()` with a `{0, 0}` timeout. The test always
passes --- if the call returns -1 with `EBADF`, this is standard behavior;
any other result is logged as a diagnostic.

**Expected Result:** The test passes regardless of the specific behavior.
The diagnostic records whether `EBADF` was returned or an alternative
handling occurred.

### Test 69 --- WaitSelect(): nfds = highest_fd + 1

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** The `nfds` parameter specifies the range of descriptors to
scan: `WaitSelect()` checks descriptors 0 through `nfds - 1`. Passing an
`nfds` value that is too low must cause the call to miss ready descriptors.
This validates correct boundary behavior.

**Methodology:** Creates a TCP loopback connection pair. Performs two
trials: In Part A, sends 10 bytes and calls `WaitSelect()` with
`nfds = server + 1` (correct value). In Part B, drains the data, sends 10
more bytes, waits 250ms for delivery, and calls `WaitSelect()` with
`nfds = server` (one too low --- excludes the server descriptor) using a
`{0, 0}` poll. Checks that Part A detects readiness (result >= 1) and
Part B misses it (result == 0).

**Expected Result:** Part A returns a value >= 1 (descriptor found). Part B
returns 0 (descriptor excluded by nfds boundary).

### Test 70 --- WaitSelect(): >64 descriptors

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** The default descriptor table size in AmiTCP is 64. Some
stacks support larger tables via `SBTC_DTABLESIZE`. This test validates that
`WaitSelect()` can handle a descriptor with a value of 64 or higher.

**Methodology:** Queries the current `SBTC_DTABLESIZE` and increases it to
128 if it is below 66. Opens 65 TCP sockets (indices 0--64). If all 65
sockets are successfully created, places the highest descriptor (index 64)
in a read set and calls `WaitSelect()` with a `{0, 0}` poll. Checks that the
call returns 0 (no error) rather than crashing or returning `EBADF`. Restores
the original descriptor table size. If 65 sockets cannot be opened, the test
is skipped.

**Expected Result:** `WaitSelect()` returns 0 when polling a descriptor
with value >= 64.

### Test 71 --- WaitSelect(): non-blocking connect completion

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** The standard pattern for non-blocking TCP connects is:
initiate `connect()`, get `EINPROGRESS`, then `WaitSelect()` for write
readiness and check `SO_ERROR`. This test validates the complete async
connect workflow.

**Methodology:** Creates a loopback listener and a TCP client socket. Sets
the client to non-blocking mode. Initiates `connect()` to the listener. If
the connect returns 0 (synchronous completion on loopback), the test passes
immediately. If the connect returns `EINPROGRESS`, calls `WaitSelect()` with
the client in the write set and a 2-second timeout. When the client becomes
writable, calls `getsockopt(SO_ERROR)` and checks that the pending error is
0 (successful connection).

**Expected Result:** The non-blocking connect completes successfully.
`WaitSelect()` reports write readiness and `SO_ERROR` is 0, or the connect
completes synchronously.

### Test 72 --- WaitSelect(): readable after peer close (EOF)

**Category:** waitselect
**API:** WaitSelect()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect)

**Rationale:** When the peer closes a connection, the local socket must
become readable so that `recv()` can return 0 (EOF). This is how
applications detect graceful disconnection via `WaitSelect()`.

**Methodology:** Creates a TCP loopback connection pair. Closes the server
socket to send a FIN. Calls `WaitSelect()` with the client in the read set
and a 2-second timeout. Checks that the client becomes readable. Calls
`recv()` on the client and checks that it returns 0 (EOF).

**Expected Result:** `WaitSelect()` reports the client as readable. `recv()`
returns 0, indicating EOF from the peer close.

---

## Category: signals

Signal and event notification tests covering `SetSocketSignals()`,
`SocketBaseTags()` signal and configuration tags, `SO_EVENTMASK`, and
`GetSocketEvents()`. These tests validate the Amiga-specific signal-driven
asynchronous notification system that replaces POSIX `SIGIO` and
`sigaction`. All 15 tests operate on loopback and require no host helper.

### Test 73 --- SetSocketSignals(): register signal masks

**Category:** signals
**API:** SetSocketSignals()
**Standard:** [AMITCP_API.md --- SetSocketSignals()](AMITCP_API.md#setsocketsignals)

**Rationale:** `SetSocketSignals()` is the legacy API for registering which
Exec signal masks the bsdsocket.library should use for SIGINTR, SIGIO, and
SIGURG notifications. A conforming implementation must accept the call
without crashing.

**Methodology:** Allocates a signal bit. Calls `SetSocketSignals()` with the
allocated signal as the SIGINTR mask and zeros for SIGIO and SIGURG. Calls
`SetSocketSignals()` again with all zeros to reset. The test always passes
if no crash occurs. Frees the signal bit.

**Expected Result:** `SetSocketSignals()` accepts the masks without error.

### Test 74 --- SocketBaseTags(SBTC_BREAKMASK): Ctrl-C signal

**Category:** signals
**API:** SocketBaseTags()
**Standard:** [AMITCP_API.md --- SocketBaseTagList()](AMITCP_API.md#socketbasetaglist--socketbasetags)

**Rationale:** `SBTC_BREAKMASK` controls which Amiga signal causes
bsdsocket.library blocking calls to abort (the Ctrl-C handler equivalent).
A conforming implementation must support set and get round-trip on this tag.

**Methodology:** Allocates a signal bit. Reads the current `SBTC_BREAKMASK`
value via `SBTM_GETREF`. Sets a new value of `1 << sigbit` via
`SBTM_SETVAL`. Reads the value back via `SBTM_GETREF`. Checks that the
read-back value matches the set value. Restores the original value and
frees the signal bit.

**Expected Result:** The `SBTC_BREAKMASK` read-back matches the value that
was set.

### Test 75 --- SocketBaseTags(SBTC_SIGEVENTMASK): event signal

**Category:** signals
**API:** SocketBaseTags()
**Standard:** [AMITCP_API.md --- SocketBaseTagList()](AMITCP_API.md#socketbasetaglist--socketbasetags)

**Rationale:** `SBTC_SIGEVENTMASK` specifies the Amiga signal that the
library sends when a socket event occurs (used with `SO_EVENTMASK` and
`GetSocketEvents()`). A conforming implementation must support set and get
round-trip on this tag.

**Methodology:** Allocates a signal bit. Reads the current
`SBTC_SIGEVENTMASK` value. Sets a new value of `1 << sigbit`. Reads the
value back. Checks that the read-back matches the set value. Clears the
mask back to 0, clears any pending signal, and frees the signal bit.

**Expected Result:** The `SBTC_SIGEVENTMASK` read-back matches the value
that was set.

### Test 76 --- SocketBaseTags(SBTC_ERRNOLONGPTR): get errno pointer

**Category:** signals
**API:** SocketBaseTags()
**Standard:** [AMITCP_API.md --- SocketBaseTagList()](AMITCP_API.md#socketbasetaglist--socketbasetags)

**Rationale:** `SBTC_ERRNOLONGPTR` registers the address of the caller's
errno variable. Reading this tag back via `SBTM_GETREF` must return the
previously registered pointer. This allows libraries to verify that errno
redirection is active.

**Methodology:** Calls `SocketBaseTags(SBTM_GETREF(SBTC_ERRNOLONGPTR))`
to retrieve the currently registered errno pointer. Checks that the returned
pointer is non-NULL (since the test framework registers an errno pointer at
startup).

**Expected Result:** The returned errno pointer is non-NULL.

### Test 77 --- SocketBaseTags(SBTC_HERRNOLONGPTR): get h_errno pointer

**Category:** signals
**API:** SocketBaseTags()
**Standard:** [AMITCP_API.md --- SocketBaseTagList()](AMITCP_API.md#socketbasetaglist--socketbasetags)

**Rationale:** `SBTC_HERRNOLONGPTR` registers the address of the caller's
h_errno variable (DNS error code). Reading this tag back must return the
registered pointer. This is the DNS-error counterpart to test 76.

**Methodology:** Calls `SocketBaseTags(SBTM_GETREF(SBTC_HERRNOLONGPTR))`
to retrieve the currently registered h_errno pointer. Checks that the
returned pointer is non-NULL.

**Expected Result:** The returned h_errno pointer is non-NULL.

### Test 78 --- SocketBaseTags(SBTC_DTABLESIZE): get/set table size

**Category:** signals
**API:** SocketBaseTags()
**Standard:** [AMITCP_API.md --- SocketBaseTagList()](AMITCP_API.md#socketbasetaglist--socketbasetags)

**Rationale:** `SBTC_DTABLESIZE` controls the maximum number of socket
descriptors available to the caller. A conforming implementation must
default to at least 64 and must support increasing the table size.

**Methodology:** Reads the current `SBTC_DTABLESIZE` via `SBTM_GETREF`.
Sets the value to 128 via `SBTM_SETVAL`. Reads the value back. Checks that
the original value was at least 64 and the new value is at least 128.
Restores the original value (the restore may not reduce the table size on
all implementations).

**Expected Result:** The initial descriptor table size is at least 64. After
setting to 128, the read-back is at least 128.

### Test 79 --- SO_EVENTMASK FD_READ: signal on data arrival

**Category:** signals
**API:** setsockopt(), GetSocketEvents()
**Standard:** [AMITCP_API.md --- SO_EVENTMASK](AMITCP_API.md#so_eventmask-socket-option),
[AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** `SO_EVENTMASK` with `FD_READ` enables signal-based
notification when data arrives on a socket. This is the primary mechanism
for Amiga applications to receive asynchronous data arrival notifications
without polling.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates a TCP loopback connection pair. Sets
`SO_EVENTMASK` to `FD_READ` on the server socket. Sends 100 bytes from the
client. Waits for the signal via `WaitSelect()` with a 2-second timeout.
Calls `GetSocketEvents()` and checks that it returns the server socket
descriptor with `FD_READ` in the event mask. Clears the event mask and
signal registration.

**Expected Result:** `GetSocketEvents()` returns the server descriptor with
`FD_READ` set.

### Test 80 --- SO_EVENTMASK FD_CONNECT: signal on connect

**Category:** signals
**API:** setsockopt(), GetSocketEvents()
**Standard:** [AMITCP_API.md --- SO_EVENTMASK](AMITCP_API.md#so_eventmask-socket-option),
[AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** `SO_EVENTMASK` with `FD_CONNECT` enables notification when a
non-blocking connect completes. This allows event-driven applications to
handle connection establishment asynchronously.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates a loopback listener and a TCP client socket.
Sets the client to non-blocking mode. Sets `SO_EVENTMASK` to `FD_CONNECT`
on the client. Initiates `connect()` to the listener. Waits for the signal.
Calls `GetSocketEvents()` and checks for the client descriptor with
`FD_CONNECT` in the event mask. If the connect completes synchronously
(returns 0), the test passes with a diagnostic noting synchronous
completion. Clears the event mask.

**Expected Result:** `GetSocketEvents()` returns the client descriptor with
`FD_CONNECT` set, or the connect completes synchronously on loopback.

### Test 81 --- SO_EVENTMASK: no spurious events on idle socket

**Category:** signals
**API:** setsockopt(), GetSocketEvents()
**Standard:** [AMITCP_API.md --- SO_EVENTMASK](AMITCP_API.md#so_eventmask-socket-option),
[AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** A socket with `SO_EVENTMASK` set but no actual activity must
not generate spurious events. This validates that the event notification
system is quiescent when no socket operations occur.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates a TCP socket (unbound, unconnected). Sets
`SO_EVENTMASK` to `FD_READ | FD_WRITE | FD_CONNECT`. Waits 100ms via
`WaitSelect()`. Checks the signal state via `SetSignal(0, 0)` (read without
clearing) and calls `GetSocketEvents()`. Checks that no signal is pending
and that `GetSocketEvents()` returns -1 (no events). Clears the event mask.

**Expected Result:** No signal is pending. `GetSocketEvents()` returns -1.

### Test 82 --- SO_EVENTMASK FD_ACCEPT: signal on incoming

**Category:** signals
**API:** setsockopt(), GetSocketEvents()
**Standard:** [AMITCP_API.md --- SO_EVENTMASK](AMITCP_API.md#so_eventmask-socket-option),
[AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** `SO_EVENTMASK` with `FD_ACCEPT` enables notification when an
incoming connection is pending on a listening socket. This allows
server applications to avoid blocking in `accept()`.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates a loopback listener. Sets `SO_EVENTMASK` to
`FD_ACCEPT` on the listener. Connects a client to the listener. Waits for
the signal via `WaitSelect()` with a 2-second timeout. Calls
`GetSocketEvents()` and checks that it returns the listener descriptor with
`FD_ACCEPT` in the event mask. Accepts the pending connection to clean up.
Clears the event mask.

**Expected Result:** `GetSocketEvents()` returns the listener descriptor
with `FD_ACCEPT` set.

### Test 83 --- SO_EVENTMASK FD_CLOSE: signal on peer disconnect

**Category:** signals
**API:** setsockopt(), GetSocketEvents()
**Standard:** [AMITCP_API.md --- SO_EVENTMASK](AMITCP_API.md#so_eventmask-socket-option),
[AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** `SO_EVENTMASK` with `FD_CLOSE` enables notification when
the remote peer disconnects. This is the signal-based equivalent of
detecting EOF via `WaitSelect()` read readiness.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates a TCP loopback connection pair. Sets
`SO_EVENTMASK` to `FD_CLOSE` on the server socket. Closes the client socket
to send a FIN. Waits for the signal via `WaitSelect()` with a 2-second
timeout. Calls `GetSocketEvents()` and checks that it returns the server
descriptor with `FD_CLOSE` in the event mask. Clears the event mask.

**Expected Result:** `GetSocketEvents()` returns the server descriptor with
`FD_CLOSE` set.

### Test 84 --- GetSocketEvents(): event consumed after retrieval

**Category:** signals
**API:** GetSocketEvents()
**Standard:** [AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** `GetSocketEvents()` must consume each event on retrieval.
Calling it a second time without new activity must return -1. This
prevents duplicate event processing.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates a TCP loopback connection pair. Sets
`SO_EVENTMASK` to `FD_READ` on the server. Sends 100 bytes from the client.
Waits for the signal. Calls `GetSocketEvents()` twice: the first call should
return the server descriptor with a valid event mask; the second call should
return -1 (no more pending events). Checks that the first call returned a
non-negative descriptor and the second call returned -1.

**Expected Result:** The first `GetSocketEvents()` call returns a valid
descriptor. The second call returns -1.

### Test 85 --- GetSocketEvents(): round-robin across sockets

**Category:** signals
**API:** GetSocketEvents()
**Standard:** [AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** When multiple sockets have pending events, successive
`GetSocketEvents()` calls must return each socket's event exactly once. This
validates fair queuing of events across descriptors.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates two independent TCP loopback connection pairs.
Sets `SO_EVENTMASK` to `FD_READ` on both server sockets. Sends 10 bytes to
each server via its corresponding client. Waits for the first signal, then
waits 100ms for the second event to propagate. Calls `GetSocketEvents()`
three times: the first two calls should return the two server descriptors
(in either order) with `FD_READ`; the third call should return -1. Checks
that both servers were reported with `FD_READ` and the third call returned
-1.

**Expected Result:** The first two `GetSocketEvents()` calls return the two
server descriptors with `FD_READ` (in either order). The third call returns
-1.

### Test 86 --- GetSocketEvents(): -1 when no events pending

**Category:** signals
**API:** GetSocketEvents()
**Standard:** [AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** When no events are pending, `GetSocketEvents()` must return
-1. This is the baseline idle-state validation.

**Methodology:** Calls `GetSocketEvents()` without any prior event setup or
signal registration. Checks that it returns -1.

**Expected Result:** `GetSocketEvents()` returns -1.

### Test 87 --- WaitSelect + signals: stress test (50 iterations)

**Category:** signals
**API:** WaitSelect(), GetSocketEvents(), setsockopt()
**Standard:** [AMITCP_API.md --- WaitSelect()](AMITCP_API.md#waitselect),
[AMITCP_API.md --- GetSocketEvents()](AMITCP_API.md#getsocketevents)

**Rationale:** Rapid cycling of the send-signal-event-recv loop exercises
the interaction between the data path, signal delivery, and event
notification under sustained load. This stress test detects race conditions
and resource leaks in the signal/event machinery.

**Methodology:** Allocates a signal bit and registers it via
`SBTC_SIGEVENTMASK`. Creates a TCP loopback connection pair. Sets
`SO_EVENTMASK` to `FD_READ` on the server and sets a 2-second receive
timeout. Runs 50 iterations of: send 10 bytes from the client, wait for
the signal via `WaitSelect()`, call `GetSocketEvents()` to verify the server
descriptor with `FD_READ`, recv 10 bytes from the server, and clear the
pending signal. If any iteration fails (send, event, or recv), the test
fails with a diagnostic identifying the failing iteration. Clears the event
mask after the loop.

**Expected Result:** All 50 iterations complete successfully with correct
event delivery and data transfer.

---

## Category: dns

Name resolution tests covering `gethostbyname()`, `gethostbyaddr()`,
`getservbyname()`, `getservbyport()`, `getprotobyname()`,
`getprotobynumber()`, `gethostname()`, `gethostid()`, `getnetbyname()`,
and `getnetbyaddr()`. Tests 88--102 operate on local databases and
loopback. Tests 103--104 require the host helper for external DNS
resolution and are skipped when no helper is connected. Several tests
conditionally skip when specific database entries (services, protocols,
networks) are not available on the running system.

### Test 88 --- gethostbyname(): "localhost" resolves to 127.0.0.1

**Category:** dns
**API:** gethostbyname()
**Standard:** [FreeBSD gethostbyname(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostbyname&sektion=3)

**Rationale:** The hostname "localhost" must resolve to the loopback
address 127.0.0.1. This is a universal requirement for any TCP/IP stack
and is the foundation of all loopback-based testing.

**Methodology:** Calls `gethostbyname("localhost")`. If the call returns a
non-NULL `hostent`, checks that `h_addrtype` is `AF_INET`, `h_length` is
4, and the first address in `h_addr_list` equals `INADDR_LOOPBACK`
(127.0.0.1). If `gethostbyname()` returns NULL, the test fails and logs
the `h_errno` value.

**Expected Result:** `gethostbyname()` returns a valid `hostent` with
`AF_INET`, length 4, and the loopback address 127.0.0.1.

### Test 89 --- gethostbyname(): invalid hostname sets h_errno

**Category:** dns
**API:** gethostbyname()
**Standard:** [FreeBSD gethostbyname(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostbyname&sektion=3)

**Rationale:** Looking up a nonexistent hostname must return NULL and set
`h_errno` to a non-zero value. This validates the DNS error reporting
path.

**Methodology:** Calls `gethostbyname("nonexistent.invalid")`. Checks that
the return value is NULL and that `get_bsd_h_errno()` returns a non-zero
error code.

**Expected Result:** `gethostbyname()` returns NULL with a non-zero
`h_errno`.

### Test 90 --- gethostbyaddr(): reverse lookup 127.0.0.1

**Category:** dns
**API:** gethostbyaddr()
**Standard:** [FreeBSD gethostbyaddr(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostbyaddr&sektion=3)

**Rationale:** Reverse DNS lookup of the loopback address is a standard
operation. The test documents the behavior without mandating a specific
hostname result, since different stacks may return "localhost", the
system hostname, or fail with a DNS error.

**Methodology:** Constructs an `in_addr` set to `INADDR_LOOPBACK` and
calls `gethostbyaddr()` with `AF_INET`. The test always passes. If the
lookup succeeds, the returned hostname is logged as a diagnostic. If it
fails, the `h_errno` value is logged.

**Expected Result:** The test passes regardless of outcome. The diagnostic
records the resolved hostname or the `h_errno` value.

### Test 91 --- gethostbyaddr(): 0.0.0.0 behavior

**Category:** dns
**API:** gethostbyaddr()
**Standard:** [FreeBSD gethostbyaddr(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostbyaddr&sektion=3)

**Rationale:** Reverse lookup of the zero address (0.0.0.0) is an edge
case. Some stacks resolve it to a hostname; others return an error. The
test documents the behavior without mandating a specific outcome.

**Methodology:** Constructs an `in_addr` set to 0 and calls
`gethostbyaddr()` with `AF_INET`. The test always passes. The result
(hostname or `h_errno`) is logged as a diagnostic.

**Expected Result:** The test passes regardless of outcome. The diagnostic
records the resolved hostname or the `h_errno` value.

### Test 92 --- getservbyname(): "http"/"tcp" -> port 80

**Category:** dns
**API:** getservbyname()
**Standard:** [FreeBSD getservbyname(3)](https://man.freebsd.org/cgi/man.cgi?query=getservbyname&sektion=3)

**Rationale:** The services database must map the well-known service name
"http" with protocol "tcp" to port 80. This validates that the stack can
read the services database.

**Methodology:** Calls `getservbyname("http", "tcp")`. If the call returns
a non-NULL `servent`, checks that `ntohs(s_port)` equals 80. If the call
returns NULL, the test is skipped with a diagnostic noting that the
services database does not include the "http" entry.

**Expected Result:** `getservbyname()` returns a `servent` with port 80,
or the test is skipped if the services database lacks the entry.

### Test 93 --- getservbyname(): unknown service returns NULL

**Category:** dns
**API:** getservbyname()
**Standard:** [FreeBSD getservbyname(3)](https://man.freebsd.org/cgi/man.cgi?query=getservbyname&sektion=3)

**Rationale:** Looking up a nonexistent service name must return NULL.
This validates the error path for service lookups.

**Methodology:** Calls `getservbyname("nonexistent_service_xyz", "tcp")`.
Checks that the return value is NULL.

**Expected Result:** `getservbyname()` returns NULL.

### Test 94 --- getservbyport(): port 21/"tcp" -> "ftp"

**Category:** dns
**API:** getservbyport()
**Standard:** [FreeBSD getservbyport(3)](https://man.freebsd.org/cgi/man.cgi?query=getservbyport&sektion=3)

**Rationale:** The reverse service lookup must map port 21 with protocol
"tcp" back to the service name "ftp". This validates bidirectional
service database access. Port 21/ftp is used rather than port 80/http
because some services databases list "www" as the primary name for port 80.

**Methodology:** Calls `getservbyport(htons(21), "tcp")`. If the call
returns a non-NULL `servent`, checks that `s_name` equals "ftp"
(case-insensitive comparison via `stricmp()`). If the call returns NULL,
the test is skipped with a diagnostic noting that the services database
does not include port 21.

**Expected Result:** `getservbyport()` returns a `servent` with name
"ftp", or the test is skipped if the services database lacks the entry.

### Test 95 --- getprotobyname(): "tcp" -> protocol 6

**Category:** dns
**API:** getprotobyname()
**Standard:** [FreeBSD getprotobyname(3)](https://man.freebsd.org/cgi/man.cgi?query=getprotobyname&sektion=3)

**Rationale:** The protocols database must map the protocol name "tcp" to
protocol number 6 (IPPROTO_TCP). This validates that the stack can read
the protocols database.

**Methodology:** Calls `getprotobyname("tcp")`. If the call returns a
non-NULL `protoent`, checks that `p_proto` equals 6. If the call returns
NULL, the test is skipped with a diagnostic noting that the protocols
database is not available.

**Expected Result:** `getprotobyname()` returns a `protoent` with
`p_proto` equal to 6, or the test is skipped if the protocols database
is unavailable.

### Test 96 --- getprotobyname(): "udp" -> protocol 17

**Category:** dns
**API:** getprotobyname()
**Standard:** [FreeBSD getprotobyname(3)](https://man.freebsd.org/cgi/man.cgi?query=getprotobyname&sektion=3)

**Rationale:** The protocols database must map the protocol name "udp" to
protocol number 17 (IPPROTO_UDP). This is the UDP counterpart to test 95.

**Methodology:** Calls `getprotobyname("udp")`. If the call returns a
non-NULL `protoent`, checks that `p_proto` equals 17. If the call returns
NULL, the test is skipped with a diagnostic noting that the protocols
database is not available.

**Expected Result:** `getprotobyname()` returns a `protoent` with
`p_proto` equal to 17, or the test is skipped if the protocols database
is unavailable.

### Test 97 --- getprotobynumber(): 6 -> "tcp"

**Category:** dns
**API:** getprotobynumber()
**Standard:** [FreeBSD getprotobynumber(3)](https://man.freebsd.org/cgi/man.cgi?query=getprotobynumber&sektion=3)

**Rationale:** The reverse protocol lookup must map protocol number 6 back
to the name "tcp". This validates bidirectional protocol database access.

**Methodology:** Calls `getprotobynumber(6)`. If the call returns a
non-NULL `protoent`, checks that `p_name` equals "tcp" (case-insensitive
comparison via `stricmp()`). If the call returns NULL, the test is skipped
with a diagnostic noting that the protocols database is not available.

**Expected Result:** `getprotobynumber()` returns a `protoent` with
`p_name` equal to "tcp", or the test is skipped if the protocols database
is unavailable.

### Test 98 --- gethostname(): retrieve hostname

**Category:** dns
**API:** gethostname()
**Standard:** [FreeBSD gethostname(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostname&sektion=3)

**Rationale:** `gethostname()` must return the local hostname as a
non-empty string. Applications use this for self-identification and
logging.

**Methodology:** Clears a 256-byte buffer and calls `gethostname()` with
that buffer. Checks that the return value is 0 and that the resulting
string has length greater than 0.

**Expected Result:** `gethostname()` returns 0 and produces a non-empty
hostname string.

### Test 99 --- gethostname(): small buffer truncation

**Category:** dns
**API:** gethostname()
**Standard:** [FreeBSD gethostname(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostname&sektion=3)

**Rationale:** When the provided buffer is too small to hold the full
hostname, the behavior varies across implementations. Some truncate
silently; others return an error. This test documents the behavior
without mandating a specific outcome.

**Methodology:** Fills a 2-byte buffer with 'X' markers and calls
`gethostname()` with that buffer. The test always passes. If the call
succeeds, the buffer contents are logged as hex values. If it fails, the
return code and errno are logged.

**Expected Result:** The test passes regardless of outcome. The diagnostic
records whether truncation occurred or an error was returned.

### Test 100 --- gethostid(): returns non-zero value

**Category:** dns
**API:** gethostid()
**Standard:** [FreeBSD gethostid(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostid&sektion=3)

**Rationale:** `gethostid()` returns a unique 32-bit identifier for the
host, typically derived from one of the host's IP addresses. A configured
system must return a non-zero value.

**Methodology:** Calls `gethostid()` and checks that the returned value
is non-zero. The value is logged as a hex diagnostic.

**Expected Result:** `gethostid()` returns a non-zero 32-bit value.

### Test 101 --- getnetbyname(): network database lookup

**Category:** dns
**API:** getnetbyname()
**Standard:** [FreeBSD getnetbyname(3)](https://man.freebsd.org/cgi/man.cgi?query=getnetbyname&sektion=3)

**Rationale:** The networks database must map the network name "loopback"
to network number 127 with address type `AF_INET`. This validates that
the stack can read the networks database.

**Methodology:** Calls `getnetbyname("loopback")`. If the call returns a
non-NULL `netent`, checks that `n_addrtype` is `AF_INET` and `n_net` is
127. If the call returns NULL, the test is skipped with a diagnostic
noting that the networks database is not available.

**Expected Result:** `getnetbyname()` returns a `netent` with `AF_INET`
and network number 127, or the test is skipped if the networks database
is unavailable.

### Test 102 --- getnetbyaddr(): network reverse lookup

**Category:** dns
**API:** getnetbyaddr()
**Standard:** [FreeBSD getnetbyaddr(3)](https://man.freebsd.org/cgi/man.cgi?query=getnetbyaddr&sektion=3)

**Rationale:** The reverse network lookup must map network number 127 with
`AF_INET` back to a named entry. This validates bidirectional network
database access.

**Methodology:** Calls `getnetbyaddr(127, AF_INET)`. If the call returns a
non-NULL `netent`, checks that `n_net` is 127, `n_name` is non-NULL, and
`n_name` has length greater than 0. If the call returns NULL, the test is
skipped with a diagnostic noting that the networks database is not
available.

**Expected Result:** `getnetbyaddr()` returns a `netent` with network
number 127 and a non-empty name, or the test is skipped if the networks
database is unavailable.

### Test 103 --- gethostbyname(): external hostname resolution

**Category:** dns
**API:** gethostbyname()
**Standard:** [FreeBSD gethostbyname(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostbyname&sektion=3)

**Rationale:** Resolving an external hostname (as opposed to "localhost")
exercises the full DNS resolution path including network communication
with a DNS server. This validates that the stack's resolver is functional
for real-world name lookups.

**Methodology:** Skipped if no host helper is connected (indicating no
network path is available). Calls `gethostbyname("aminet.net")`. If the
call returns a non-NULL `hostent`, checks that `h_addrtype` is `AF_INET`
and `h_length` is 4. The resolved address is logged as a diagnostic. If
the call returns NULL, the test fails and logs the `h_errno` value.

**Expected Result:** `gethostbyname()` returns a valid `hostent` with
`AF_INET` and length 4.

### Test 104 --- gethostbyaddr(): external reverse lookup

**Category:** dns
**API:** gethostbyaddr()
**Standard:** [FreeBSD gethostbyaddr(3)](https://man.freebsd.org/cgi/man.cgi?query=gethostbyaddr&sektion=3)

**Rationale:** Reverse DNS lookup of an external IP address exercises the
PTR record resolution path. This complements test 103 by testing the
reverse direction.

**Methodology:** Skipped if no host helper is connected. Uses the host
helper's IP address (from `helper_addr()`) as the lookup target. Calls
`gethostbyaddr()` with that address. If the lookup succeeds, checks that
`h_addrtype` is `AF_INET` and `h_length` is 4, and logs the resolved
hostname. If the lookup returns NULL, the test passes unconditionally and
the `h_errno` value is logged.

**Expected Result:** When the lookup succeeds, `gethostbyaddr()` returns a
valid `hostent` with `h_addrtype` equal to `AF_INET` and `h_length`
equal to 4. When the lookup fails (reverse DNS not available), the test
passes unconditionally.

---

## Category: utility

Address conversion and manipulation utility tests covering `Inet_NtoA()`,
`inet_addr()`, `Inet_LnaOf()`, `Inet_NetOf()`, `Inet_MakeAddr()`, and
`inet_network()`. All 10 tests are pure computation with no socket I/O
and require no network access or host helper.

### Test 105 --- Inet_NtoA(): 127.0.0.1 formatting

**Category:** utility
**API:** Inet_NtoA()
**Standard:** [AMITCP_API.md --- Inet_NtoA()](AMITCP_API.md#inet_ntoa)

**Rationale:** `Inet_NtoA()` must convert a network-byte-order IPv4
address to its dotted-decimal string representation. The loopback address
is the most fundamental test case.

**Methodology:** Calls `Inet_NtoA(htonl(0x7f000001))` and checks that the
returned string is non-NULL and equal to "127.0.0.1" (exact string
comparison via `strcmp()`).

**Expected Result:** `Inet_NtoA()` returns the string "127.0.0.1".

### Test 106 --- Inet_NtoA(): 255.255.255.255 formatting

**Category:** utility
**API:** Inet_NtoA()
**Standard:** [AMITCP_API.md --- Inet_NtoA()](AMITCP_API.md#inet_ntoa)

**Rationale:** The broadcast address 255.255.255.255 exercises the maximum
value for each octet. This validates correct formatting when all bits are
set.

**Methodology:** Calls `Inet_NtoA(htonl(0xffffffff))` and checks that the
returned string is non-NULL and equal to "255.255.255.255".

**Expected Result:** `Inet_NtoA()` returns the string "255.255.255.255".

### Test 107 --- Inet_NtoA(): 0.0.0.0 formatting

**Category:** utility
**API:** Inet_NtoA()
**Standard:** [AMITCP_API.md --- Inet_NtoA()](AMITCP_API.md#inet_ntoa)

**Rationale:** The zero address exercises the minimum value for each
octet. This validates correct formatting when all bits are clear.

**Methodology:** Calls `Inet_NtoA(0)` (network byte order zero is the
same as host byte order zero) and checks that the returned string is
non-NULL and equal to "0.0.0.0".

**Expected Result:** `Inet_NtoA()` returns the string "0.0.0.0".

### Test 108 --- inet_addr(): parse "127.0.0.1"

**Category:** utility
**API:** inet_addr()
**Standard:** [FreeBSD inet_addr(3)](https://man.freebsd.org/cgi/man.cgi?query=inet_addr&sektion=3)

**Rationale:** `inet_addr()` must parse a dotted-decimal string into its
network-byte-order binary representation. The loopback address is the
most fundamental test case.

**Methodology:** Calls `inet_addr("127.0.0.1")` and checks that the
returned value equals `htonl(0x7f000001)`.

**Expected Result:** `inet_addr()` returns `htonl(0x7f000001)`.

### Test 109 --- inet_addr(): invalid string returns INADDR_NONE

**Category:** utility
**API:** inet_addr()
**Standard:** [FreeBSD inet_addr(3)](https://man.freebsd.org/cgi/man.cgi?query=inet_addr&sektion=3)

**Rationale:** Parsing an invalid dotted-decimal string must return
`INADDR_NONE` (0xffffffff). This validates the error reporting path.

**Methodology:** Calls `inet_addr("not.an.ip")` and checks that the
returned value equals `INADDR_NONE`.

**Expected Result:** `inet_addr()` returns `INADDR_NONE`.

### Test 110 --- inet_addr(): "255.255.255.255"

**Category:** utility
**API:** inet_addr()
**Standard:** [FreeBSD inet_addr(3)](https://man.freebsd.org/cgi/man.cgi?query=inet_addr&sektion=3)

**Rationale:** The broadcast address "255.255.255.255" is a well-known
ambiguity in the `inet_addr()` API: its binary representation
(0xffffffff) is the same value as `INADDR_NONE`. This test documents the
behavior and notes the ambiguity.

**Methodology:** Calls `inet_addr("255.255.255.255")` and checks that the
returned value equals 0xffffffff. A diagnostic note is emitted about the
`INADDR_NONE` ambiguity with the broadcast address.

**Expected Result:** `inet_addr()` returns 0xffffffff.

### Test 111 --- Inet_LnaOf(): extract host part

**Category:** utility
**API:** Inet_LnaOf()
**Standard:** [AMITCP_API.md --- Inet_LnaOf()](AMITCP_API.md#inet_lnaof)

**Rationale:** `Inet_LnaOf()` extracts the host (local network address)
portion of an IPv4 address based on its classful network class. For a
Class A address (10.x.x.x), the host part is the lower 24 bits.

**Methodology:** Calls `Inet_LnaOf(htonl(0x0a010203))` on the Class A
address 10.1.2.3. Checks that the returned host part equals 0x010203.

**Expected Result:** `Inet_LnaOf()` returns 0x010203.

### Test 112 --- Inet_NetOf(): extract network part

**Category:** utility
**API:** Inet_NetOf()
**Standard:** [AMITCP_API.md --- Inet_NetOf()](AMITCP_API.md#inet_netof)

**Rationale:** `Inet_NetOf()` extracts the network portion of an IPv4
address based on its classful network class. For a Class A address
(10.x.x.x), the network part is the upper 8 bits.

**Methodology:** Calls `Inet_NetOf(htonl(0x0a010203))` on the Class A
address 10.1.2.3. Checks that the returned network part equals 0x0a.

**Expected Result:** `Inet_NetOf()` returns 0x0a.

### Test 113 --- Inet_MakeAddr(): round-trip with LnaOf/NetOf

**Category:** utility
**API:** Inet_MakeAddr()
**Standard:** [AMITCP_API.md --- Inet_MakeAddr()](AMITCP_API.md#inet_makeaddr)

**Rationale:** `Inet_MakeAddr()` must reconstruct an IPv4 address from
its network and host parts. When combined with `Inet_NetOf()` and
`Inet_LnaOf()`, the result must be the original address. This validates
the round-trip consistency of the three classful address decomposition
functions.

**Methodology:** Extracts the network part via `Inet_NetOf(htonl(0x0a010203))`
and the host part via `Inet_LnaOf(htonl(0x0a010203))`. Calls
`Inet_MakeAddr(net, host)` to reconstruct the address. Checks that the
rebuilt address equals `htonl(0x0a010203)`.

**Expected Result:** `Inet_MakeAddr()` returns `htonl(0x0a010203)`,
matching the original address.

### Test 114 --- inet_network(): host byte order conversion

**Category:** utility
**API:** inet_network()
**Standard:** [FreeBSD inet_network(3)](https://man.freebsd.org/cgi/man.cgi?query=inet_network&sektion=3)

**Rationale:** `inet_network()` parses a dotted-decimal string and returns
the network number in host byte order. Unlike `inet_addr()`, which
returns network byte order, `inet_network()` is used for classful network
number operations.

**Methodology:** Calls `inet_network("10.0.0.0")` and checks that the
returned value equals 0x0a000000 (host byte order).

**Expected Result:** `inet_network()` returns 0x0a000000.

---

## Category: transfer

Socket descriptor transfer tests covering `Dup2Socket()`,
`ReleaseSocket()`, `ObtainSocket()`, and `ReleaseCopyOfSocket()`. These
are Amiga-specific functions that allow socket descriptors to be
duplicated within a process or transferred between processes via a shared
socket pool. All 5 tests operate on loopback and require no host helper.

### Test 115 --- Dup2Socket(fd, -1): duplicate to new descriptor

**Category:** transfer
**API:** Dup2Socket()
**Standard:** [AMITCP_API.md --- Dup2Socket()](AMITCP_API.md#dup2socket)

**Rationale:** `Dup2Socket(fd, -1)` duplicates a socket to a new
descriptor chosen by the library, similar to BSD `dup()`. The new
descriptor must be valid and distinct from the original.

**Methodology:** Creates a TCP socket. Calls `Dup2Socket(fd, -1)`.
Checks that the returned descriptor is non-negative and different from
the original descriptor. Closes both descriptors.

**Expected Result:** `Dup2Socket()` returns a non-negative descriptor
that is not equal to the original.

### Test 116 --- Dup2Socket(fd, target): duplicate to specific slot

**Category:** transfer
**API:** Dup2Socket()
**Standard:** [AMITCP_API.md --- Dup2Socket()](AMITCP_API.md#dup2socket)

**Rationale:** `Dup2Socket(fd, target)` duplicates a socket to a specific
descriptor number, similar to BSD `dup2()`. This is used by daemon
processes that need descriptors at specific slot numbers.

**Methodology:** Creates a TCP socket. Sets the target to `fd + 10`. Calls
`Dup2Socket(fd, target)`. If the returned descriptor equals the target,
the test passes. If the call returns -1 (the stack does not support
targeting a specific slot), the test also passes with a diagnostic noting
the limitation. Any other result is a failure.

**Expected Result:** `Dup2Socket()` returns the requested target
descriptor, or -1 if specific-slot duplication is not supported.

### Test 117 --- Dup2Socket(): duplicated descriptor can send/recv

**Category:** transfer
**API:** Dup2Socket()
**Standard:** [AMITCP_API.md --- Dup2Socket()](AMITCP_API.md#dup2socket)

**Rationale:** A duplicated socket descriptor must be fully functional for
data transfer. This validates that the duplicate shares the same
underlying socket state as the original.

**Methodology:** Creates a TCP loopback connection pair. Duplicates the
server descriptor via `Dup2Socket(server, -1)`. Sends 100 bytes with test
pattern seed 115 from the client. Receives on the duplicated descriptor
with a 2-second timeout. Checks that exactly 100 bytes were received and
the pattern matches.

**Expected Result:** `recv()` on the duplicated descriptor returns 100
bytes with correct data.

### Test 118 --- ReleaseSocket()/ObtainSocket(): same-process roundtrip

**Category:** transfer
**API:** ReleaseSocket(), ObtainSocket()
**Standard:** [AMITCP_API.md --- ReleaseSocket()](AMITCP_API.md#releasesocket),
[AMITCP_API.md --- ObtainSocket()](AMITCP_API.md#obtainsocket)

**Rationale:** `ReleaseSocket()` places a socket into the library's
transfer pool with a unique ID, invalidating the original descriptor.
`ObtainSocket()` retrieves it by ID, creating a new local descriptor.
Although designed for inter-process transfer, same-process round-trip
validates the mechanism without requiring a second process.

**Methodology:** Creates a TCP loopback connection pair. Sends 100 bytes
with test pattern seed 116 from the client while the server socket is
still active. Calls `ReleaseSocket(server, 42)` to place the server
socket in the transfer pool with unique ID 42. The original server
descriptor is invalidated. Calls `ObtainSocket(released_id, AF_INET,
SOCK_STREAM, 0)` to retrieve the socket into a new descriptor. Sets a
2-second receive timeout on the obtained descriptor and receives. Checks
that 100 bytes were received with correct pattern data. If `ReleaseSocket`
returns -1, the test is skipped (the function is not supported).

**Expected Result:** `ObtainSocket()` returns a valid descriptor that can
receive the 100 bytes sent before the release.

### Test 119 --- ReleaseCopyOfSocket(): original remains usable

**Category:** transfer
**API:** ReleaseCopyOfSocket()
**Standard:** [AMITCP_API.md --- ReleaseCopyOfSocket()](AMITCP_API.md#releasecopyofsocket)

**Rationale:** Unlike `ReleaseSocket()`, `ReleaseCopyOfSocket()` places a
copy of the socket into the transfer pool while leaving the original
descriptor valid and usable. This test validates that the original
continues to function normally after the copy is released.

**Methodology:** Creates a TCP loopback connection pair. Calls
`ReleaseCopyOfSocket(server, 43)` to place a copy in the pool. Sends 100
bytes with test pattern seed 117 from the client. Receives on the
original server descriptor with a 2-second timeout. Checks that 100 bytes
were received with correct pattern data. The copy is abandoned in the
pool and cleaned up at library close. If `ReleaseCopyOfSocket` returns -1,
the test is skipped (the function is not supported).

**Expected Result:** The original server descriptor remains functional.
`recv()` returns 100 bytes with correct data after the copy was released.

---

## Category: errno

Error handling tests covering `Errno()`, `SetErrnoPtr()`, and
`SocketBaseTags(SBTC_ERRNOLONGPTR)`. These tests validate the
bsdsocket.library error reporting mechanisms, including the Amiga-specific
ability to redirect errno writes to variables of different sizes (1, 2,
or 4 bytes). All 7 tests operate on loopback and require no host helper.

### Test 120 --- Errno(): correct value after failed operation

**Category:** errno
**API:** Errno()
**Standard:** [AMITCP_API.md --- Errno()](AMITCP_API.md#errno)

**Rationale:** After a failed bsdsocket.library call, `Errno()` must
return the same non-zero error code as the registered errno variable.
This validates the basic error reporting path.

**Methodology:** Calls `socket(-1, -1, -1)` which is guaranteed to fail.
Calls `Errno()` to retrieve the error code. Checks that the socket call
returned a negative value, that `Errno()` returned a non-zero value, and
that `Errno()` matches `get_bsd_errno()` (the value read from the
registered errno variable).

**Expected Result:** `Errno()` returns a non-zero value that matches
`get_bsd_errno()`.

### Test 121 --- Errno(): behavior after successful operation

**Category:** errno
**API:** Errno()
**Standard:** [AMITCP_API.md --- Errno()](AMITCP_API.md#errno)

**Rationale:** BSD does not guarantee that errno is cleared on success.
Some implementations clear it; others leave the previous value. This test
documents the behavior without mandating a specific outcome.

**Methodology:** Calls `CloseSocket(-1)` to set errno to a non-zero value.
Then calls `socket(AF_INET, SOCK_STREAM, 0)` which should succeed. Calls
`Errno()` to read the error code. The test always passes. If the value is
0, a diagnostic notes "errno cleared on success". If non-zero, a
diagnostic notes the residual value.

**Expected Result:** The test passes regardless of outcome. The diagnostic
records whether errno was cleared or retained after the successful call.

### Test 122 --- SetErrnoPtr(): 1-byte variable

**Category:** errno
**API:** SetErrnoPtr()
**Standard:** [AMITCP_API.md --- SetErrnoPtr()](AMITCP_API.md#seterrnoptr)

**Rationale:** `SetErrnoPtr()` allows redirecting errno writes to a
variable of a specified size. With size 1, the library must write the
error code as a single byte. This tests the narrowest supported width.

**Methodology:** Declares a `BYTE` variable initialized to 0. Calls
`SetErrnoPtr(&err_byte, 1)` to redirect errno to this variable. Calls
`CloseSocket(-1)` to trigger an error. Checks that the byte variable is
non-zero. Restores the original errno pointer via `restore_bsd_errno()`.

**Expected Result:** The 1-byte errno variable is set to a non-zero value
after the failed operation.

### Test 123 --- SetErrnoPtr(): 2-byte variable

**Category:** errno
**API:** SetErrnoPtr()
**Standard:** [AMITCP_API.md --- SetErrnoPtr()](AMITCP_API.md#seterrnoptr)

**Rationale:** With size 2, `SetErrnoPtr()` must write the error code as
a 16-bit word. This tests the medium-width errno variable.

**Methodology:** Declares a `WORD` variable initialized to 0. Calls
`SetErrnoPtr(&err_word, 2)` to redirect errno to this variable. Calls
`CloseSocket(-1)` to trigger an error. Checks that the word variable is
non-zero. Restores the original errno pointer via `restore_bsd_errno()`.

**Expected Result:** The 2-byte errno variable is set to a non-zero value
after the failed operation.

### Test 124 --- SetErrnoPtr(): 4-byte variable

**Category:** errno
**API:** SetErrnoPtr()
**Standard:** [AMITCP_API.md --- SetErrnoPtr()](AMITCP_API.md#seterrnoptr)

**Rationale:** With size 4, `SetErrnoPtr()` must write the error code as
a 32-bit longword. This is the standard width and the most commonly used
configuration.

**Methodology:** Declares a `LONG` variable initialized to 0. Calls
`SetErrnoPtr(&err_long, 4)` to redirect errno to this variable. Calls
`CloseSocket(-1)` to trigger an error. Checks that the long variable is
non-zero. Restores the original errno pointer via `restore_bsd_errno()`.

**Expected Result:** The 4-byte errno variable is set to a non-zero value
after the failed operation.

### Test 125 --- SBTC_ERRNOLONGPTR: error updates pointed-to variable

**Category:** errno
**API:** SocketBaseTags()
**Standard:** [AMITCP_API.md --- SocketBaseTagList()](AMITCP_API.md#socketbasetaglist--socketbasetags)

**Rationale:** `SBTC_ERRNOLONGPTR` registers a `LONG *` for the library
to write error codes into. Two different failing operations must produce
two different non-zero error values in the pointed-to variable. This
validates that the library actively updates the registered variable on
each error.

**Methodology:** Declares a `LONG` variable and registers it via
`SocketBaseTags(SBTM_SETVAL(SBTC_ERRNOLONGPTR))`. Calls `CloseSocket(-1)`
(expected to produce `EBADF`) and saves the variable's value. Then creates
a TCP socket and calls `connect()` to a non-listening loopback port
(expected to produce `ECONNREFUSED`) and saves the variable's value again.
Checks that both values are non-zero and that they differ from each other.
Restores the original errno pointer via `restore_bsd_errno()`.

**Expected Result:** The registered variable is updated to a non-zero
value after each failing operation, and the two error values are
different.

### Test 126 --- connect(): not affected by stale errno

**Category:** errno
**API:** connect(), CloseSocket()
**Standard:** [POSIX --- errno](https://pubs.opengroup.org/onlinepubs/9699919799/functions/errno.html)

**Rationale:** POSIX specifies that errno is only meaningful after a function
that returns an error indication. A stale errno value from a prior failed
operation must not influence the outcome of subsequent calls. This test
verifies that `connect()` succeeds on a valid socket even when errno contains
`EBADF` from a prior `CloseSocket(-1)` call.

**Methodology:** Creates a loopback listener on a test port. Creates a TCP
socket for connecting. Calls `CloseSocket(-1)` to force errno to `EBADF`.
Calls `Errno()` to confirm the stale state is non-zero.
Calls `connect()` on the valid socket to the loopback listener. Checks that
`connect()` returns 0 (success). Logs the stale errno value, the connect
return code, and the post-connect errno.

**Expected Result:** `connect()` returns 0 (success) regardless of stale
errno state.

---

## Category: misc

Miscellaneous tests covering `getdtablesize()`, `syslog()`,
`CloseSocket()` after `shutdown()`, and resource-limit behavior. These
tests validate peripheral library functions and edge cases in socket
lifecycle management. All 5 tests operate on loopback and require no
host helper.

### Test 127 --- getdtablesize(): default descriptor table size

**Category:** misc
**API:** getdtablesize()
**Standard:** [AMITCP_API.md --- getdtablesize()](AMITCP_API.md#getdtablesize)

**Rationale:** Every bsdsocket.library opener has a descriptor table that
limits the number of simultaneously open sockets. `getdtablesize()`
reports the current table size. A conforming implementation must return
at least 64, the minimum required by the AmiTCP API.

**Methodology:** Calls `getdtablesize()` and checks that the returned
value is greater than or equal to 64. Emits the actual value as a
diagnostic.

**Expected Result:** `getdtablesize()` returns a value of 64 or greater.

### Test 128 --- getdtablesize(): reflects SBTC_DTABLESIZE change

**Category:** misc
**API:** getdtablesize()
**Standard:** [AMITCP_API.md --- getdtablesize()](AMITCP_API.md#getdtablesize)

**Rationale:** The descriptor table size can be increased at runtime via
`SocketBaseTags(SBTC_DTABLESIZE)`. After enlarging the table,
`getdtablesize()` must reflect the new size. This validates that the
configuration and query functions are consistent.

**Methodology:** Reads the current descriptor table size via
`SocketBaseTags(SBTM_GETREF(SBTC_DTABLESIZE))` and saves it. Sets a new
size 64 larger than the original via `SBTM_SETVAL(SBTC_DTABLESIZE)`.
Calls `getdtablesize()` and checks that the returned value is at least
as large as the requested new size. Restores the original table size
afterward.

**Expected Result:** `getdtablesize()` returns a value greater than or
equal to the newly requested table size.

### Test 129 --- syslog(): does not crash (canary test)

**Category:** misc
**API:** vsyslog()
**Standard:** [AMITCP_API.md --- vsyslog()](AMITCP_API.md#vsyslog)

**Rationale:** The `syslog()` convenience macro may be broken in certain
SDK versions (missing `_sfdc_vararg` symbol). This canary test validates
that the underlying `vsyslog()` function can be called without crashing,
even if log output cannot be easily verified.

**Methodology:** Sets the log tag to `"bsdsocktest"` via
`SocketBaseTags(SBTM_SETVAL(SBTC_LOGTAGPTR))`. Calls `vsyslog()` with
`LOG_INFO` priority and a format string containing a `%s` argument. The
argument array is constructed manually as a `ULONG[]` to match the
AmigaOS varargs convention, bypassing the broken `syslog()` macro. The
test unconditionally passes if the call returns without crashing.

**Expected Result:** The `vsyslog()` call returns without crashing.

### Test 130 --- CloseSocket(): succeeds after prior shutdown

**Category:** misc
**API:** CloseSocket()
**Standard:** [AMITCP_API.md --- CloseSocket()](AMITCP_API.md#closesocket)

**Rationale:** Calling `shutdown(SHUT_RDWR)` disables both halves of a
connection but does not release the descriptor. A subsequent
`CloseSocket()` must still succeed and return 0 to release the
descriptor. This validates that `CloseSocket()` handles the
already-shutdown state correctly.

**Methodology:** Creates a TCP loopback connection (listener, client,
server). Calls `shutdown(client, 2)` (SHUT_RDWR) to fully shut down the
client side. Then calls `CloseSocket()` on the client descriptor and
checks that it returns 0. Closes the remaining server and listener
descriptors.

**Expected Result:** `CloseSocket()` returns 0 on a descriptor that has
already been fully shut down.

### Test 131 --- socket(): open dtablesize-1 descriptors successfully

**Category:** misc
**API:** socket()
**Standard:** [AMITCP_API.md --- getdtablesize()](AMITCP_API.md#getdtablesize)

**Rationale:** The descriptor table size reported by `getdtablesize()`
defines how many sockets can be open simultaneously. A conforming
implementation must allow opening at least `dtablesize - 1` descriptors
(reserving one slot for internal use is acceptable). This stress test
validates the table capacity under load.

**Methodology:** Queries `getdtablesize()` to determine the table
capacity. Opens TCP sockets in a loop up to `dtablesize - 1` (capped at
256), recording each descriptor. Checks that at least 32 sockets were
successfully opened. Reports the actual count as a diagnostic and a
`tap_note`. Closes all sockets in reverse order.

**Expected Result:** At least 32 TCP sockets are successfully opened.
The reported count is informational --- the exact maximum depends on the
stack's descriptor table configuration.

---

## Category: icmp

ICMP echo (ping) tests using raw sockets. These tests validate the
ability to create `SOCK_RAW` / `IPPROTO_ICMP` sockets, construct and
send ICMP echo requests, and receive matching echo replies. RTT
measurements are informational benchmarks, not conformance assertions.
All 5 tests are skipped if the stack does not support raw ICMP sockets.
Tests 133--135 additionally require the host helper for network targets.

### Test 132 --- ICMP echo: loopback 127.0.0.1

**Category:** icmp
**API:** socket(), sendto(), recv()
**Standard:** [RFC 792](https://www.rfc-editor.org/rfc/rfc792)

**Rationale:** An ICMP echo request to the loopback address must produce
a matching echo reply. This is the most basic raw socket ping test and
validates that the stack handles ICMP over loopback without requiring
network access.

**Methodology:** Creates a `SOCK_RAW` / `IPPROTO_ICMP` socket.
Constructs an ICMP echo request packet with a 56-byte payload, a unique
identifier (`0xBD51`), and sequence number 1. Computes the ICMP
checksum. Sends the packet to `127.0.0.1` via `sendto()`. Enters a
receive loop using `WaitSelect()` with a 3-second timeout, reading
packets and checking for an ICMP echo reply matching the identifier and
sequence number. Returns the round-trip time in microseconds measured via
`timer.device`. Reports the RTT as a diagnostic and a `tap_note`.

**Expected Result:** A matching ICMP echo reply is received within 3
seconds. The test passes if the measured RTT is positive.

### Test 133 --- ICMP echo: network host

**Category:** icmp
**API:** socket(), sendto(), recv()
**Standard:** [RFC 792](https://www.rfc-editor.org/rfc/rfc792)

**Rationale:** An ICMP echo request sent to a remote host (the host
helper) must produce a matching echo reply, validating that the stack
correctly handles ICMP over the network interface rather than just
loopback.

**Methodology:** Skipped if the host helper is not connected. Sends an
ICMP echo request with a 56-byte payload and sequence number 2 to the
host helper's IP address using the `icmp_ping()` helper function. The
receive loop uses `WaitSelect()` with a 3-second timeout. Validates the
reply's identifier and sequence number. Reports the RTT as a diagnostic
and a `tap_note`.

**Expected Result:** A matching ICMP echo reply is received from the
network host within 3 seconds.

### Test 134 --- ICMP echo: 1024-byte payload

**Category:** icmp
**API:** socket(), sendto(), recv()
**Standard:** [RFC 792](https://www.rfc-editor.org/rfc/rfc792)

**Rationale:** ICMP echo with a larger payload (1024 bytes) tests the
stack's ability to handle non-trivial ICMP packet sizes. The echo reply
must include the original payload data, and the payload integrity is
verified against the deterministic test pattern.

**Methodology:** Skipped if the host helper is not connected. Sends an
ICMP echo request with a 1024-byte payload (filled with a deterministic
test pattern seeded by sequence number 3) to the host helper's IP
address. On reply, verifies that the returned payload matches the
original pattern using `verify_test_pattern()`. Reports any payload
mismatch offset as a diagnostic.

**Expected Result:** A matching ICMP echo reply is received with
intact 1024-byte payload data.

### Test 135 --- ICMP echo: multiple pings reliability

**Category:** icmp
**API:** socket(), sendto(), recv()
**Standard:** [RFC 792](https://www.rfc-editor.org/rfc/rfc792)

**Rationale:** A single successful ping does not demonstrate reliable
ICMP processing. Sending multiple echo requests in sequence and
requiring a high reply rate validates that the stack handles repeated
ICMP transactions without dropping packets or corrupting state.

**Methodology:** Skipped if the host helper is not connected. Sends 5
ICMP echo requests to the host helper with 56-byte payloads and
sequential sequence numbers (10--14). Each request uses a fresh raw
socket with a 3-second timeout. Counts the number of successful replies
and tracks the minimum, maximum, and average RTT across all replies.
Checks that at least 4 out of 5 replies were received. Reports the reply
count and RTT statistics as diagnostics and a `tap_note`.

**Expected Result:** At least 4 out of 5 ICMP echo replies are received
from the network host.

### Test 136 --- ICMP echo: timeout on unreachable host

**Category:** icmp
**API:** socket(), sendto(), WaitSelect()
**Standard:** [RFC 792](https://www.rfc-editor.org/rfc/rfc792)

**Rationale:** When sending an ICMP echo request to an unreachable
address, the stack must either time out (no reply) or return an
immediate error such as `ENETUNREACH`. It must not return a spurious
reply. This validates negative-path behavior for raw ICMP sockets.

**Methodology:** Sends an ICMP echo request with a 56-byte payload and
sequence number 99 to `192.0.2.1` (TEST-NET-1, a documentation-only
address block defined in RFC 5737 that should never be routable). Waits
up to 3 seconds for a reply. The test passes if `icmp_ping()` returns 0
(timeout) or -1 (send error, e.g., `ENETUNREACH` when no default route
exists). Fails only if a reply is unexpectedly received.

**Expected Result:** No ICMP echo reply is received. The `icmp_ping()`
helper returns 0 (timeout) or -1 (send error).

---

## Category: throughput

TCP and UDP throughput benchmark tests measuring data transfer rates on
loopback and across the network. These are performance measurements, not
conformance assertions --- the tests pass as long as data was
successfully transferred. Throughput numbers are reported as informational
TAP diagnostics and notes. Network tests (138, 140, 142) require the
host helper.

### Test 137 --- Throughput: TCP loopback send/recv

**Category:** throughput
**API:** send(), recv(), WaitSelect()
**Standard:** Performance benchmark (no conformance standard)

**Rationale:** Measures TCP throughput over loopback to establish a
baseline for the stack's internal data transfer performance. Since
loopback avoids NIC and network overhead, this reflects the stack's
pure software path efficiency.

**Methodology:** Creates a TCP loopback connection (listener, client,
server). Sets both endpoints to non-blocking mode. Transfers 512 KB from
client to server using a `WaitSelect()` event loop: the client sends
8 KB chunks when the socket is writable, and the server receives into an
8 KB buffer when the socket is readable. After all data is sent, the
client calls `shutdown(SHUT_WR)` to signal EOF. The loop continues until
the server has received all data or a 10-second timeout expires.
Measures elapsed time via `timer.device` and computes throughput as
`(received_bytes / 1024) * 1000 / elapsed_ms` (KB/s). Passes if at
least 90% of the target bytes were received.

**Expected Result:** At least 460 KB (90% of 512 KB) is received by the
server. The reported throughput in KB/s is informational.

### Test 138 --- Throughput: TCP via network to host

**Category:** throughput
**API:** send()
**Standard:** Performance benchmark (no conformance standard)

**Rationale:** Measures TCP send throughput to a remote host, reflecting
real-world network transfer performance including NIC, bridge, and
network stack overhead.

**Methodology:** Skipped if the host helper is not connected. Connects
to the host helper's TCP sink service (port 8703), which accepts and
discards all received data. Sends 512 KB in 8 KB chunks using blocking
`send()` calls. Measures elapsed time via `timer.device` and computes
throughput as `(sent_bytes / 1024) * 1000 / elapsed_ms` (KB/s). Passes
if any data was successfully sent.

**Expected Result:** At least some data is sent to the host helper's
TCP sink. The reported throughput in KB/s is informational.

### Test 139 --- Throughput: UDP loopback

**Category:** throughput
**API:** sendto(), recv(), WaitSelect()
**Standard:** Performance benchmark (no conformance standard)

**Rationale:** Measures UDP datagram throughput over loopback, providing
a baseline for the stack's UDP processing path. Unlike TCP, UDP has no
flow control, so this test also implicitly measures the stack's internal
buffering capacity.

**Methodology:** Creates two UDP sockets bound to loopback on separate
ports. Sends 200 datagrams of 1024 bytes each from socket A to socket B
using `sendto()`, filling each with a deterministic test pattern. After
all sends complete, sets socket B to non-blocking mode and enters a
receive loop using `WaitSelect()` with a 1-second timeout, draining all
available datagrams. Measures total elapsed time and computes throughput
as `(received_count * 1024 / 1024) * 1000 / elapsed_ms` (KB/s). Reports
the send count, receive count, loss percentage, and throughput. Passes
if at least one datagram was received.

**Expected Result:** At least one UDP datagram is received. The reported
throughput, loss percentage, and datagram counts are informational.

### Test 140 --- Throughput: UDP via network to host

**Category:** throughput
**API:** sendto(), recv(), WaitSelect()
**Standard:** Performance benchmark (no conformance standard)

**Rationale:** Measures UDP round-trip throughput across the network by
sending datagrams to the host helper's UDP echo service and receiving
the echoed replies. This reflects real-world UDP performance including
network latency and potential packet loss.

**Methodology:** Skipped if the host helper is not connected. Creates a
UDP socket. Sends 200 datagrams of 1024 bytes each to the host helper's
UDP echo service (port 8702) using `sendto()`. After all sends, sets the
socket to non-blocking mode and enters a receive loop using
`WaitSelect()` with a 1-second timeout, counting echoed replies.
Measures total elapsed time and computes throughput. Reports the send
count, echo count, loss percentage, and throughput. Passes if at least
one echoed reply was received.

**Expected Result:** At least one echoed UDP datagram is received. The
reported throughput and loss percentage are informational.

### Test 141 --- Throughput: TCP sustained 1MB+ loopback

**Category:** throughput
**API:** send(), recv(), WaitSelect()
**Standard:** Performance benchmark (no conformance standard)

**Rationale:** A sustained transfer of 1 MB or more reveals throughput
stability over time. By dividing the transfer into 100 KB segments and
timing each independently, this test detects throughput degradation,
stalls, or buffer management issues that shorter transfers might miss.

**Methodology:** Creates a TCP loopback connection and sets both
endpoints to non-blocking mode. Transfers 1 MB from client to server
using the same `WaitSelect()` event loop as test 137 (8 KB send/recv
buffers, 10-second timeout). Divides the transfer into 10 segments of
100 KB each, recording the elapsed time for each segment at send-side
boundaries. After the transfer completes, reports overall throughput and
per-segment diagnostics including the minimum and maximum segment times
and per-segment KB/s rates. Passes if all 1 MB is received by the
server.

**Expected Result:** All 1 MB (1,048,576 bytes) is received by the
server. The reported overall and per-segment throughput values are
informational.

### Test 142 --- Throughput: TCP sustained 1MB+ via network

**Category:** throughput
**API:** send()
**Standard:** Performance benchmark (no conformance standard)

**Rationale:** A sustained network transfer of 1 MB or more tests
real-world throughput stability, including the interaction between the
Amiga TCP stack, the NIC driver, and the network. Per-segment timing
reveals whether throughput degrades under sustained load.

**Methodology:** Skipped if the host helper is not connected. Connects
to the host helper's TCP sink service (port 8703). Sends 1 MB in 8 KB
chunks using blocking `send()` calls. Divides the transfer into 10
segments of 100 KB each, recording the elapsed time at each segment
boundary on the send side. After the transfer completes, reports overall
throughput and per-segment diagnostics including the minimum and maximum
segment times and per-segment KB/s rates. Passes if all 1 MB is sent.

**Expected Result:** All 1 MB (1,048,576 bytes) is sent to the host
helper's TCP sink. The reported overall and per-segment throughput values
are informational.
