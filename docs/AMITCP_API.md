# Amiga bsdsocket.library API Reference

## Introduction

This document is a programmer's reference for the Amiga **bsdsocket.library**
--- the shared library that provides BSD socket networking on AmigaOS. It
covers functions and mechanisms that differ from or extend the standard BSD
socket API as defined by POSIX.1 and 4.4BSD.

The bsdsocket.library API was introduced by AmiTCP/IP and has since been
implemented by multiple Amiga TCP/IP stacks (Miami, MiamiDx, Roadshow,
Genesis) and by emulators (Amiberry, WinUAE). The 46-function core API at
LVO offsets -30 through -300 is common to all implementations.

Standard BSD functions (socket, bind, listen, connect, accept, send, recv,
and so on) are available through the library with the same semantics as their
POSIX counterparts. Those functions are listed in a quick-reference table
in a later section of this document, with links to the corresponding
FreeBSD and POSIX man pages. This reference focuses on the areas where the
Amiga implementation diverges: library lifecycle, Amiga-specific replacement
functions, the tag-based configuration interface, and Amiga signal
integration.

### Conventions

- Function prototypes use the types as they appear in the AmiTCP SDK headers:
  `LONG`, `ULONG`, `STRPTR`, `APTR`, `UBYTE`, `struct Library *`, and so on.
  `LONG` is a signed 32-bit integer; `ULONG` is unsigned 32-bit.

- All functions require `SocketBase` to be set (via `OpenLibrary()`) before
  use. Calling any bsdsocket.library function with a NULL `SocketBase` will
  crash.

- Register assignments listed for each function refer to the 68k library
  call convention: parameters are placed in the specified data (D0-D7) or
  address (A0-A5) registers, and the library base is always in A6. The
  return value, when present, is in D0.

- LVO (Library Vector Offset) numbers are negative offsets from the library
  base pointer. They are fixed across all conforming implementations.

- Where `struct timeval` is referenced, the Amiga-native field names
  `tv_secs` and `tv_micro` apply (from `<devices/timer.h>`). The SDK's
  `<devices/timer.h>` defines `struct timeval` with anonymous unions, so
  both the BSD field names (`tv_sec`, `tv_usec`) and the Amiga-native
  names (`tv_secs`, `tv_micro`) are valid and access the same storage.

- The document is organized by functional area: library initialization,
  socket management, I/O, socket options, name resolution, signals and
  asynchronous notification, and utility functions.


## Library Initialization

Every AmigaOS task (process) that uses bsdsocket.library must open the
library independently and maintain its own `SocketBase` pointer. The inline
stub macros generated from the library's FD file reference `SocketBase`
by name, so it must be declared as a global variable.

The library also maintains a per-opener errno value that is separate from the
C runtime's `errno`. Unless the caller registers a pointer to a local
variable via `SocketBaseTags()`, socket errors will be silently lost.

### Opening the Library

```c
#include <proto/exec.h>
#include <proto/bsdsocket.h>
#include <amitcp/socketbasetags.h>

struct Library *SocketBase = NULL;   /* must be global */
static LONG bsd_errno = 0;
static LONG bsd_h_errno = 0;
```

Open with a minimum version of 4 to request the AmiTCP 4.x API (46
functions, LVO -30 through -300):

```c
SocketBase = OpenLibrary("bsdsocket.library", 4);
if (!SocketBase) {
    /* No TCP/IP stack is running */
    return;
}
```

Immediately after opening, register errno and h_errno storage so that
error codes from socket operations are written to variables the program
can inspect:

```c
SocketBaseTags(
    SBTM_SETVAL(SBTC_ERRNOLONGPTR), (ULONG)&bsd_errno,
    SBTM_SETVAL(SBTC_HERRNOLONGPTR), (ULONG)&bsd_h_errno,
    TAG_DONE
);
```

`SBTM_SETVAL()` and `SBTC_ERRNOLONGPTR` are macros defined in
`<amitcp/socketbasetags.h>`. The tag codes encode direction (get/set),
passing convention (value/reference), and a function code. See the
`SocketBaseTagList()` entry (in a later section) for the full tag
reference.

### Closing the Library

```c
CloseLibrary(SocketBase);
SocketBase = NULL;
```

All sockets obtained from the library should be closed with `CloseSocket()`
before calling `CloseLibrary()`. The behavior of sockets left open at
`CloseLibrary()` time is implementation-defined; some stacks leak them,
others clean up automatically.

### Compiler Requirements

The inline stubs in `<proto/bsdsocket.h>` (which includes
`<inline/bsdsocket.h>`) use GCC statement expressions --- a GNU C
extension. Programs must be compiled with GNU C mode. Do not pass
`-std=c99` or `-std=c11` to the compiler, as these disable statement
expressions and cause compilation failures. The `-noixemul` flag (libnix
runtime) is recommended for standalone Amiga executables.

### Per-Task Semantics

Each AmigaOS task or process that calls socket functions must open
bsdsocket.library separately. The library maintains per-opener state
including the descriptor table, errno pointers, signal masks, and syslog
configuration. Sharing a `SocketBase` across tasks leads to undefined
behavior.

### errno vs. C Library errno

The bsdsocket.library errno is entirely separate from the C library's
`errno` (whether libnix, ixemul, or clib2). A call to `socket()` that
fails sets the bsdsocket errno --- not the C `errno`. If the caller has not
registered an errno pointer via `SocketBaseTags()`, there is no way to
retrieve the error code. The legacy `SetErrnoPtr()` function (LVO -168)
also registers an errno pointer but is superseded by the tag-based
interface.


## Socket Management

This section covers functions for closing sockets and querying the
descriptor table. These are Amiga-specific: `CloseSocket()` replaces the
POSIX `close()` system call, and `getdtablesize()` queries the
library's descriptor table capacity.

---

### CloseSocket()

**Library:** bsdsocket.library
**LVO:** -120
**Registers:** D0

**Synopsis:**

```c
#include <proto/bsdsocket.h>

LONG CloseSocket(LONG sock);
```

**Description:**

`CloseSocket()` closes a socket descriptor previously obtained from
`socket()`, `accept()`, `ObtainSocket()`, or `Dup2Socket()`. It releases
all resources associated with the descriptor within the bsdsocket.library's
per-opener descriptor table.

This function must be used instead of the C library `close()`. Socket
descriptors are managed by bsdsocket.library in a namespace separate from
AmigaOS file handles. Passing a socket descriptor to `close()`, or passing
an AmigaOS file handle to `CloseSocket()`, produces undefined behavior.

For TCP (SOCK_STREAM) sockets with pending data, the kernel may linger to
complete transmission depending on the SO_LINGER socket option. If
SO_LINGER is set with a non-zero timeout, `CloseSocket()` may block until
data is sent or the timeout expires.

A socket that has been partially shut down with `shutdown()` can still be
closed with `CloseSocket()`. The close succeeds regardless of the shutdown
state.

**Parameters:**

- `sock` --- The socket descriptor to close. Must be a valid descriptor
  in the caller's descriptor table.

**Return Value:**

Returns 0 on success. Returns -1 on failure and sets the bsdsocket errno.

**Errors:**

- `EBADF` --- `sock` is not a valid open socket descriptor.

**Notes:**

- After `CloseSocket()` returns successfully, the descriptor number may
  be reused by a subsequent `socket()`, `accept()`, or `Dup2Socket()`
  call.
- Closing an invalid descriptor (such as -1, or a descriptor that has
  already been closed) returns an error but does not crash.
- `CloseSocket()` should be called for every socket before closing the
  library with `CloseLibrary()`. Leaked sockets may persist within the
  stack and exhaust descriptor table entries for subsequent program runs
  within the same library context.

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to the POSIX `close()` system call
when applied to socket descriptors. Named `CloseSocket()` rather than
`close()` because Amiga shared libraries cannot override C library
functions.

**See Also:**

`socket()`, `shutdown()`, `Dup2Socket()`,
[close(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=close&sektion=2)

---

### getdtablesize()

**Library:** bsdsocket.library
**LVO:** -138
**Registers:** (none)

**Synopsis:**

```c
#include <proto/bsdsocket.h>

LONG getdtablesize(void);
```

**Description:**

`getdtablesize()` returns the current size of the per-opener socket
descriptor table. This is the maximum number of socket descriptors that the
calling task can have open simultaneously. Descriptor numbers range from 0
to `getdtablesize() - 1`.

The default table size is 64 on most implementations. It can be changed
at runtime using `SocketBaseTags()` with the `SBTC_DTABLESIZE` tag code:

```c
/* Increase the descriptor table to 128 entries */
SocketBaseTags(SBTM_SETVAL(SBTC_DTABLESIZE), 128, TAG_DONE);
```

After setting a new size, `getdtablesize()` reflects the change. Note
that most implementations only allow the table to grow, not shrink. An
attempt to reduce the table size below the current value may be silently
ignored or clamped.

The current value can also be read through the tag interface:

```c
LONG dtsize = 0;
SocketBaseTags(SBTM_GETREF(SBTC_DTABLESIZE), (ULONG)&dtsize, TAG_DONE);
```

**Parameters:**

(none)

**Return Value:**

Returns the current descriptor table size as a positive LONG. The value is
always at least 64 on conforming implementations.

**Errors:**

This function does not fail and does not set errno.

**Notes:**

- The descriptor table is per-opener (per-task), not system-wide. Each
  task that opens bsdsocket.library gets its own table.
- The table size limits only socket descriptors managed by
  bsdsocket.library. It has no effect on AmigaOS file handles.
- The maximum table size is implementation-defined. AmiTCP and Roadshow
  support at least 256. Amiberry's emulation layer may have different
  limits.
- Opening `getdtablesize() - 1` sockets simultaneously is expected to
  succeed. The last slot is sometimes reserved internally.

**Conformance:**

AmiTCP SDK 4.0. Equivalent to the 4.2BSD `getdtablesize()` system call.
The BSD function was deprecated by POSIX in favor of `sysconf(_SC_OPEN_MAX)`
and `getrlimit(RLIMIT_NOFILE)`, neither of which is available on AmigaOS.

**See Also:**

`SocketBaseTagList()`, `SBTC_DTABLESIZE`,
[getdtablesize(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getdtablesize&sektion=3)


## Asynchronous I/O

This section covers functions for multiplexed socket I/O and Amiga signal
integration. `WaitSelect()` is the Amiga replacement for the BSD `select()`
system call, extended with a sixth parameter for waiting on Amiga signals
alongside socket descriptors. `SetSocketSignals()` is a legacy function
for registering signal masks with the library.

---

### WaitSelect()

**Library:** bsdsocket.library
**LVO:** -126
**Registers:** D0/A0/A1/A2/A3/D1

**Synopsis:**

```c
#include <proto/bsdsocket.h>

LONG WaitSelect(LONG nfds, APTR read_fds, APTR write_fds,
                APTR except_fds, struct timeval *timeout,
                ULONG *signals);
```

**Description:**

`WaitSelect()` monitors a set of socket descriptors for readiness
conditions (readable, writable, or exception) and optionally waits for
Amiga signals simultaneously. It is the Amiga-specific extension of the
BSD `select()` system call, with a sixth parameter (`signals`) that
integrates the Exec signal mechanism into the blocking wait.

When called, `WaitSelect()` examines the socket descriptors specified in
the three descriptor sets (`read_fds`, `write_fds`, `except_fds`) to see
if any are ready for the corresponding I/O operation. If none are ready,
the function blocks until at least one descriptor becomes ready, a timeout
expires, or an Amiga signal in the specified mask arrives --- whichever
occurs first.

The sixth parameter is what distinguishes `WaitSelect()` from POSIX
`select()`. By passing a pointer to a signal mask, a program can wait
for socket activity and AmigaOS events (such as a Ctrl-C break signal or
an asynchronous notification from another task) in a single blocking call.
This eliminates the need for polling loops or separate threads to handle
both socket I/O and Amiga signals.

**Parameters:**

- `nfds` --- The number of descriptor slots to examine. This must be set
  to the numerically highest socket descriptor in any of the three sets,
  plus one. It is not the count of descriptors in the sets. Descriptors
  numbered 0 through `nfds - 1` are examined; any descriptors at or above
  `nfds` are ignored. For example, if the highest descriptor in the sets
  is 5, `nfds` must be 6.

- `read_fds` --- Pointer to an `fd_set` (declared as `APTR` in the library
  prototype; the cast is implicit when passing `&fdset`). Specifies the
  descriptors to monitor for read readiness. A descriptor is read-ready when data is
  available to be received, a connection has been accepted on a listening
  socket, or the peer has closed the connection (EOF). May be NULL if
  read readiness is not of interest.

- `write_fds` --- Pointer to an `fd_set` specifying the descriptors to
  monitor for write readiness. A descriptor is write-ready when send
  buffer space is available, or a non-blocking `connect()` has completed.
  May be NULL.

- `except_fds` --- Pointer to an `fd_set` specifying the descriptors to
  monitor for exceptional conditions. The primary use is detecting the
  arrival of TCP out-of-band (OOB) data. May be NULL.

- `timeout` --- Pointer to a `struct timeval` specifying the maximum time
  to block. Three cases apply:

  - **NULL**: Block indefinitely until a descriptor becomes ready or a
    signal arrives (if `signals` is provided).
  - **Zero value** (`tv_secs = 0`, `tv_micro = 0`): Do not block; poll
    the descriptors and return immediately.
  - **Non-zero value**: Block for at most the specified duration.

  The implementation may modify the `timeval` structure to reflect the
  time remaining (i.e., decrement it by the elapsed wait time). Callers
  that need the original timeout value should save a copy before calling
  `WaitSelect()`.

  If all three descriptor set pointers are NULL and `timeout` is non-NULL,
  `WaitSelect()` acts as a pure delay function, blocking for the duration
  specified by `timeout` (or until a signal arrives). In this mode, `nfds`
  should be 0.

- `signals` --- Pointer to a `ULONG` containing an Amiga signal mask, or
  NULL. This is the Amiga-specific extension.

  The behavior depends on the value pointed to:

  - **NULL**: Pure BSD `select()` behavior. The function blocks only on
    socket descriptor readiness and timeout.
  - **Pointer to zero**: Same as NULL. The function does not wait for
    any Amiga signals.
  - **Pointer to non-zero mask**: The function also returns when any of
    the Amiga signals specified in the mask are received by the calling
    task. On return, the `ULONG` pointed to by `signals` is modified to
    contain only the signals that actually fired. This allows the caller
    to distinguish which signal caused the return.

**Return Value:**

- **Positive value**: The total number of socket descriptors that are
  ready across all three sets. The descriptor sets are modified in place
  to contain only the descriptors that are ready.

- **Zero**: No descriptors are ready. This occurs in three cases:

  - The timeout expired before any descriptor became ready.
  - An Amiga signal from the `signals` mask arrived. In this case, the
    descriptor sets are zeroed (no descriptors are ready), and `*signals`
    is modified to contain only the signals that fired.
  - A zero timeout was specified (poll mode) and no descriptors were
    ready at the time of the call.

- **-1**: An error occurred. The descriptor sets and signal mask are
  undefined. The bsdsocket errno is set to indicate the error.

**Errors:**

- `EBADF` --- One of the descriptor sets contains an invalid socket
  descriptor (one that is not open or is out of range).

- `EINTR` --- The call was interrupted by a signal registered via
  `SBTC_BREAKMASK` (typically Ctrl-C). Note that signals delivered via
  the `signals` parameter do not produce `EINTR`; they produce a normal
  return of 0.

- `EINVAL` --- `nfds` is negative, or the `timeout` value is invalid
  (negative `tv_secs` or `tv_micro` outside the range 0--999999).

**Notes:**

- The `fd_set` type is a bitmask representing a set of socket descriptors.
  It is manipulated exclusively through the following macros:

  - `FD_ZERO(&set)` --- Initialize the set to empty.
  - `FD_SET(fd, &set)` --- Add descriptor `fd` to the set.
  - `FD_CLR(fd, &set)` --- Remove descriptor `fd` from the set.
  - `FD_ISSET(fd, &set)` --- Test whether descriptor `fd` is in the set.
    Returns non-zero if present.

  `FD_SETSIZE` defines the maximum number of descriptors an `fd_set` can
  hold. It defaults to 256 in the AmiTCP headers. Descriptors numbered
  at or above `FD_SETSIZE` cannot be represented and must not be used
  with `WaitSelect()`.

- The `struct timeval` on AmigaOS uses the field names `tv_secs` (seconds)
  and `tv_micro` (microseconds), as defined in `<devices/timer.h>`. The
  SDK's `<devices/timer.h>` defines `struct timeval` with anonymous unions,
  so both the BSD names (`tv_sec`, `tv_usec`) and the Amiga-native names
  (`tv_secs`, `tv_micro`) are valid and access the same storage.

- `WaitSelect()` can serve as a general-purpose delay mechanism. Calling
  it with `nfds` set to 0, all three descriptor sets set to NULL, and a
  non-NULL `timeout` causes it to block for the specified duration:

  ```c
  struct timeval tv;
  tv.tv_secs = 0;
  tv.tv_micro = 250000;  /* 250 ms */
  WaitSelect(0, NULL, NULL, NULL, &tv, NULL);
  ```

- When using `WaitSelect()` with Amiga signals, the typical pattern is:

  ```c
  BYTE sigbit = AllocSignal(-1);
  ULONG sigmask = 1UL << sigbit;

  /* ... set up socket descriptors ... */

  rc = WaitSelect(nfds, &readfds, NULL, NULL, NULL, &sigmask);
  if (rc > 0) {
      /* Socket(s) ready --- check FD_ISSET */
  } else if (rc == 0 && sigmask) {
      /* Amiga signal arrived --- check which bit(s) */
  } else if (rc == 0) {
      /* Timeout expired */
  } else {
      /* Error */
  }

  FreeSignal(sigbit);
  ```

- Signals delivered through the `signals` parameter do not set errno to
  `EINTR`. The `EINTR` error is reserved for signals registered via
  `SocketBaseTags()` with `SBTC_BREAKMASK`, which represent an
  interruption rather than a normal event.

- On a signal return (return value 0 with `*signals` non-zero), the
  descriptor sets should be treated as zeroed. No descriptors are
  reported as ready.

- On a descriptor readiness return (return value positive), the signal
  mask pointed to by `signals` may be left unchanged or may be replaced
  with the set of signals that were pending (which may be zero). Callers
  should not depend on the value of `*signals` when the return value is
  positive.

- A listening socket (one on which `listen()` has been called) becomes
  readable when a connection is pending in the backlog queue and can be
  accepted with `accept()`.

- A socket on which a non-blocking `connect()` is in progress becomes
  writable when the connection completes. After `WaitSelect()` indicates
  write readiness, the caller should use `getsockopt()` with `SO_ERROR`
  to determine whether the connection succeeded or failed.

- A peer closing its end of a TCP connection causes the local socket to
  become readable. A subsequent `recv()` returns 0 (EOF).

**Conformance:**

AmiTCP SDK 4.0. Extends the BSD `select()` system call with the sixth
`signals` parameter for Amiga signal integration. The first five
parameters and their semantics are identical to 4.4BSD `select()`.

**See Also:**

`SetSocketSignals()`, `SocketBaseTagList()`, `GetSocketEvents()`,
`SBTC_BREAKMASK`, `SBTC_SIGEVENTMASK`, `SBTC_SIGIOMASK`,
`SBTC_SIGURGMASK`,
[select(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=select&sektion=2)

---

### SetSocketSignals()

**Library:** bsdsocket.library
**LVO:** -132
**Registers:** D0/D1/D2

**Synopsis:**

```c
#include <proto/bsdsocket.h>

VOID SetSocketSignals(ULONG int_mask, ULONG io_mask,
                      ULONG urgent_mask);
```

**Description:**

`SetSocketSignals()` registers three Amiga signal masks with the
bsdsocket.library for the calling task's library context. These masks
control which signals the TCP/IP stack delivers to the task in response
to specific socket events:

- `int_mask` --- The interrupt (break) signal mask. Signals in this mask
  cause blocking socket operations to be interrupted with an `EINTR`
  error. This is typically set to `SIGBREAKF_CTRL_C` to allow Ctrl-C
  to abort a blocked `WaitSelect()` or other long-running call.

- `io_mask` --- The I/O signal mask. Signals in this mask are delivered
  when asynchronous I/O events occur on sockets that have been configured
  for asynchronous notification.

- `urgent_mask` --- The urgent data signal mask. Signals in this mask are
  delivered when TCP urgent (out-of-band) data arrives on a socket.

All three parameters are Amiga signal masks (bitmasks where bit N
corresponds to signal number N, obtained from `AllocSignal()`). Passing
0 for any mask disables that category of signal delivery.

This function is **superseded** by the tag-based interface. The same functionality is available
through `SocketBaseTags()` using the following tag codes:

- `SBTC_BREAKMASK` --- Equivalent to `int_mask`.
- `SBTC_SIGIOMASK` --- Equivalent to `io_mask`.
- `SBTC_SIGURGMASK` --- Equivalent to `urgent_mask`.

The tag-based interface is preferred because it supports both get and set
operations and can be combined with other configuration tags in a single
call. `SetSocketSignals()` can only set the masks and provides no way to
query their current values.

`SetSocketSignals()` remains functional in all conforming implementations
and is tested for backward compatibility.

**Parameters:**

- `int_mask` --- Signal mask for interrupt/break signals. Typically
  `SIGBREAKF_CTRL_C` or 0.
- `io_mask` --- Signal mask for asynchronous I/O notification signals,
  or 0 to disable.
- `urgent_mask` --- Signal mask for TCP urgent data notification signals,
  or 0 to disable.

**Return Value:**

None (VOID).

**Errors:**

This function does not fail and does not set errno.

**Notes:**

- The masks set by `SetSocketSignals()` are per-opener state. They
  affect only the calling task's bsdsocket.library context.
- Calling `SetSocketSignals(0, 0, 0)` clears all three masks,
  disabling all signal-based notifications.
- The tag equivalents offer additional flexibility:

  ```c
  /* Read current break mask, then set a new one */
  ULONG old_mask = 0;
  SocketBaseTags(
      SBTM_GETREF(SBTC_BREAKMASK), (ULONG)&old_mask,
      SBTM_SETVAL(SBTC_BREAKMASK), SIGBREAKF_CTRL_C,
      TAG_DONE
  );
  ```

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists; this is an
Amiga-specific function. Superseded by `SocketBaseTags()` with
`SBTC_BREAKMASK`, `SBTC_SIGIOMASK`, and `SBTC_SIGURGMASK`.

**See Also:**

`WaitSelect()`, `SocketBaseTagList()`, `SBTC_BREAKMASK`,
`SBTC_SIGIOMASK`, `SBTC_SIGURGMASK`


## Event Notification

The bsdsocket.library provides an asynchronous event notification
mechanism that is separate from `WaitSelect()`. Instead of polling for
readiness on a set of descriptors, a program can register interest in
specific event types on individual sockets. When a matching event occurs,
the library delivers an Amiga signal to the calling task. The task then
calls `GetSocketEvents()` to retrieve the socket descriptor and event
mask.

This mechanism requires three pieces of configuration:

1. An Amiga signal mask registered via `SocketBaseTags()` with the
   `SBTC_SIGEVENTMASK` tag code. This tells the library which signal
   to deliver when an event occurs.

2. A per-socket event mask set via `setsockopt()` with the
   `SO_EVENTMASK` option. This tells the library which event types
   to monitor on that socket.

3. A call to `GetSocketEvents()` after the signal arrives, to retrieve
   the descriptor and event details.

---

### SO_EVENTMASK (socket option)

**Library:** bsdsocket.library (via `setsockopt()` / `getsockopt()`)
**Option Level:** `SOL_SOCKET` (0xFFFF)
**Option Name:** `SO_EVENTMASK` (0x2001)

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <libraries/bsdsocket.h>

LONG mask = FD_READ | FD_CLOSE;
setsockopt(sock, SOL_SOCKET, SO_EVENTMASK, &mask, sizeof(mask));

LONG current = 0;
socklen_t len = sizeof(current);
getsockopt(sock, SOL_SOCKET, SO_EVENTMASK, &current, &len);
```

**Description:**

`SO_EVENTMASK` is a socket-level option (opcode 0x2001) that configures
which asynchronous events the library should track for a given socket.
When an event matching the mask occurs, the library delivers the signal
specified by `SBTC_SIGEVENTMASK` to the calling task. The pending event
can then be retrieved with `GetSocketEvents()`.

The option value is a bitmask of `FD_*` event type constants, defined in
`<libraries/bsdsocket.h>`:

| Constant     | Value  | Meaning                                      |
|--------------|--------|----------------------------------------------|
| `FD_ACCEPT`  | 0x01   | A connection is pending and can be accepted   |
| `FD_CONNECT` | 0x02   | A non-blocking `connect()` has completed      |
| `FD_OOB`     | 0x04   | Out-of-band (urgent) data has arrived         |
| `FD_READ`    | 0x08   | Data is available to be read                  |
| `FD_WRITE`   | 0x10   | Send buffer space is available                |
| `FD_ERROR`   | 0x20   | An asynchronous error has occurred            |
| `FD_CLOSE`   | 0x40   | The connection has been closed (graceful or not) |

Setting `SO_EVENTMASK` to 0 disables event notification for that socket.

This option is marked "private" in the Roadshow `<sys/socket.h>` header
with a comment stating it should not be used by user code. In practice,
it is the standard mechanism for asynchronous event notification across
all conforming implementations and is widely used by applications.

**Parameters:**

- `sock` --- The socket descriptor to configure.
- `level` --- Must be `SOL_SOCKET` (0xFFFF).
- `optname` --- Must be `SO_EVENTMASK` (0x2001).
- `optval` --- Pointer to a LONG containing the desired event mask (for
  `setsockopt()`), or pointer to a LONG to receive the current mask
  (for `getsockopt()`).
- `optlen` --- `sizeof(LONG)` (4 bytes).

**Return Value:**

`setsockopt()` and `getsockopt()` return 0 on success, -1 on failure.

**Errors:**

- `EBADF` --- `sock` is not a valid socket descriptor.
- `ENOPROTOOPT` --- The option is not recognized (should not occur on
  conforming implementations).

**Notes:**

- `SO_EVENTMASK` is per-socket state. Each socket can have a different
  event mask, and events are tracked independently.

- The `SBTC_SIGEVENTMASK` signal must be registered before
  `SO_EVENTMASK` will have any effect. If no signal mask is registered,
  the library has no signal to deliver when events occur.

- The typical usage pattern is:

  ```c
  BYTE sigbit = AllocSignal(-1);
  ULONG sigmask = 1UL << sigbit;

  /* Register the event signal */
  SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), sigmask, TAG_DONE);

  /* Enable FD_READ events on this socket */
  LONG mask = FD_READ;
  setsockopt(sock, SOL_SOCKET, SO_EVENTMASK, &mask, sizeof(mask));

  /* Wait for the signal */
  sigmask = 1UL << sigbit;
  WaitSelect(0, NULL, NULL, NULL, NULL, &sigmask);

  /* Retrieve the event */
  ULONG evmask = 0;
  LONG evfd = GetSocketEvents(&evmask);
  if (evfd == sock && (evmask & FD_READ)) {
      /* Data is available */
  }

  /* Cleanup: disable events before freeing signal */
  mask = 0;
  setsockopt(sock, SOL_SOCKET, SO_EVENTMASK, &mask, sizeof(mask));
  SocketBaseTags(SBTM_SETVAL(SBTC_SIGEVENTMASK), 0, TAG_DONE);
  SetSignal(0, 1UL << sigbit);
  FreeSignal(sigbit);
  ```

- Always clear `SO_EVENTMASK` to 0 on each socket and clear
  `SBTC_SIGEVENTMASK` to 0 before freeing the signal bit. Failure
  to do so can cause signal races where the library delivers a signal
  to a bit that has been freed or reallocated.

- An idle socket with `SO_EVENTMASK` set does not generate spurious
  events. The signal is delivered only when a matching event actually
  occurs.

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists. This is an
Amiga-specific socket option that integrates with the Exec signal
mechanism. It is analogous in purpose to the Windows Sockets
`WSAAsyncSelect()` function.

**See Also:**

`GetSocketEvents()`, `SocketBaseTagList()`, `SBTC_SIGEVENTMASK`,
`setsockopt()`, `getsockopt()`

---

### GetSocketEvents()

**Library:** bsdsocket.library
**LVO:** -300
**Registers:** A0

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <libraries/bsdsocket.h>

LONG GetSocketEvents(ULONG *event_ptr);
```

**Description:**

`GetSocketEvents()` retrieves the next pending asynchronous event from
the library's per-opener event queue. It is the retrieval side of the
event notification mechanism; `SO_EVENTMASK` and `SBTC_SIGEVENTMASK`
are the configuration side.

When the library delivers an event signal (via `SBTC_SIGEVENTMASK`),
one or more events are queued internally. Each call to
`GetSocketEvents()` dequeues and returns one event. The function
writes the event type bitmask to the location pointed to by `event_ptr`
and returns the socket descriptor on which the event occurred.

Events are consumed by retrieval. A second call to `GetSocketEvents()`
after the first returns a different event (if one is pending) or -1 if
the queue is empty. This is a destructive read --- once an event is
retrieved, it cannot be retrieved again.

When multiple sockets have pending events simultaneously, successive
calls to `GetSocketEvents()` return them in round-robin order, one
socket per call, until all pending events have been consumed.

**Parameters:**

- `event_ptr` --- Pointer to a ULONG that receives the event bitmask.
  On return, this contains a combination of `FD_*` constants indicating
  which events occurred on the returned socket. If no event is pending,
  the value is left unchanged or set to 0 (implementation-defined).

**Return Value:**

- **Non-negative value**: The socket descriptor on which the event
  occurred. The `*event_ptr` value contains the event mask.

- **-1**: No events are pending. The event queue is empty.

**Errors:**

This function does not set errno.

**Notes:**

- `GetSocketEvents()` is the last function in the core AmiTCP 4.x API
  (LVO -300, function 46 of 46). It was added in AmiTCP 4.0 alongside
  `SocketBaseTagList()`.

- The event queue is per-opener (per-task). Events from other tasks'
  sockets are never visible.

- After receiving the event signal via `WaitSelect()` or `Wait()`, the
  caller should loop on `GetSocketEvents()` until it returns -1 to drain
  all pending events:

  ```c
  ULONG evmask;
  LONG evfd;

  for (;;) {
      evmask = 0;
      evfd = GetSocketEvents(&evmask);
      if (evfd == -1)
          break;
      /* Handle event on socket evfd with mask evmask */
  }
  ```

- The event mask returned in `*event_ptr` may contain multiple bits
  set simultaneously if more than one event type occurred on the same
  socket before `GetSocketEvents()` was called.

- Calling `GetSocketEvents()` when no events are pending (and no
  `SO_EVENTMASK` has been set on any socket) is safe and returns -1
  immediately.

- The event signal (`SBTC_SIGEVENTMASK`) may remain asserted in the
  task's signal set after all events are consumed. Callers using
  `WaitSelect()` with the signal in the `signals` parameter should
  clear it with `SetSignal(0, sigmask)` after draining the event
  queue to avoid false wakeups on the next `WaitSelect()` call.

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists. This is an
Amiga-specific function.

**See Also:**

`SO_EVENTMASK`, `SocketBaseTagList()`, `SBTC_SIGEVENTMASK`,
`WaitSelect()`


## Library Configuration

The bsdsocket.library provides a tag-based configuration interface
through `SocketBaseTagList()` and its variadic wrapper `SocketBaseTags()`.
This interface is the primary mechanism for configuring per-opener state:
errno registration, signal masks, descriptor table size, syslog
parameters, error string lookup, and feature queries.

---

### SocketBaseTagList() / SocketBaseTags()

**Library:** bsdsocket.library
**LVO:** -294
**Registers:** A0

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <libraries/bsdsocket.h>
#include <amitcp/socketbasetags.h>

LONG SocketBaseTagList(struct TagItem *tags);
LONG SocketBaseTags(Tag first_tag, ...);
```

**Description:**

`SocketBaseTagList()` processes an array of `struct TagItem` entries
to get or set per-opener configuration parameters of the
bsdsocket.library. Each tag item specifies a parameter code, an access
direction (get or set), a passing convention (by value or by reference),
and the associated data.

`SocketBaseTags()` is a variadic convenience macro (defined in
`<inline/bsdsocket.h>`) that constructs a `TagItem` array on the stack
from its arguments and calls `SocketBaseTagList()`. It is the preferred
form for inline use. The array is terminated by `TAG_DONE` (value 0).

Multiple tag items can be passed in a single call, allowing several
parameters to be read or written atomically. Tags are processed in
order. If a tag is not recognized or an error occurs processing a
specific tag, the function may stop processing subsequent tags and
return an error count.

#### Tag Encoding Scheme

Each tag value passed to `SocketBaseTagList()` encodes four pieces of
information in a single ULONG:

```
Bit 31:    TAG_USER (always set, value 0x80000000)
Bit 15:    Passing convention (0 = by value, 1 = by reference)
Bits 14-1: Parameter code (SBTC_* value, shifted left by 1)
Bit 0:     Direction (0 = GET, 1 = SET)
```

The encoding is constructed using four macros defined in both
`<libraries/bsdsocket.h>` and `<amitcp/socketbasetags.h>`:

- **`SBTM_GETVAL(code)`** --- Read a parameter; the current value is
  returned in `ti_Data` (the tag item's data field). Used for values
  that fit in a ULONG.

  Formula: `TAG_USER | SBTF_VAL | ((code & SBTS_CODE) << SBTB_CODE) | SBTF_GET`

- **`SBTM_GETREF(code)`** --- Read a parameter; `ti_Data` is a pointer
  to a ULONG where the current value is written. Used when the value
  must be stored in caller-provided memory.

  Formula: `TAG_USER | SBTF_REF | ((code & SBTS_CODE) << SBTB_CODE) | SBTF_GET`

- **`SBTM_SETVAL(code)`** --- Write a parameter; `ti_Data` contains the
  new value directly.

  Formula: `TAG_USER | SBTF_VAL | ((code & SBTS_CODE) << SBTB_CODE) | SBTF_SET`

- **`SBTM_SETREF(code)`** --- Write a parameter; `ti_Data` is a pointer
  to the new value.

  Formula: `TAG_USER | SBTF_REF | ((code & SBTS_CODE) << SBTB_CODE) | SBTF_SET`

Where the bit-field constants are:

| Constant    | Value  | Meaning                                  |
|-------------|--------|------------------------------------------|
| `SBTF_VAL`  | 0x0000 | `ti_Data` contains the value directly    |
| `SBTF_REF`  | 0x8000 | `ti_Data` is a pointer to the value      |
| `SBTB_CODE` | 1      | Bit position of the code field           |
| `SBTS_CODE`  | 0x3FFF | Mask for the code field (14 bits)        |
| `SBTF_GET`  | 0      | Read operation                           |
| `SBTF_SET`  | 1      | Write operation                          |

The helper macro `SBTM_CODE(tag)` extracts the SBTC_* code from a
constructed tag value: `((tag >> SBTB_CODE) & SBTS_CODE)`.

#### SBTC_* Tag Codes

The following table lists all tag codes defined in the SDK headers.
Codes are grouped by functional area. The "Access" column indicates
whether the code supports GET, SET, or both.

**Signal masks** (per-opener, ULONG bitmasks):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_BREAKMASK`       | 1     | GET/SET  | Interrupt/break signal mask. Signals in this mask cause blocking operations to return with `EINTR`. Typically set to `SIGBREAKF_CTRL_C`. |
| `SBTC_SIGIOMASK`       | 2     | GET/SET  | Asynchronous I/O signal mask. Delivered when async I/O events occur on sockets configured with `FIOASYNC`. |
| `SBTC_SIGURGMASK`      | 3     | GET/SET  | Urgent data signal mask. Delivered when TCP out-of-band data arrives. |
| `SBTC_SIGEVENTMASK`    | 4     | GET/SET  | Event notification signal mask. Delivered when an event matching a socket's `SO_EVENTMASK` occurs. Used with `GetSocketEvents()`. Added in AmiTCP 4.0. |

**Error handling** (per-opener):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_ERRNO`           | 6     | GET/SET  | Current errno value. GET returns the last socket error code. SET allows manually setting the errno (rarely needed). |
| `SBTC_HERRNO`          | 7     | GET/SET  | Current h_errno value (resolver error code). |

**Descriptor table** (per-opener):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_DTABLESIZE`      | 8     | GET/SET  | Size of the socket descriptor table. Default is 64. Can be increased but usually not decreased below the current value. |

**File descriptor callback** (per-opener, legacy):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_FDCALLBACK`      | 9     | GET/SET  | Link library fd allocation callback function pointer. The callback has the prototype `int fdCallback(int fd, int action)` with `fd` in D0 and `action` in D1. Actions: `FDCB_FREE` (0), `FDCB_ALLOC` (1), `FDCB_CHECK` (2). **Do not use in new code.** |

**Syslog configuration** (per-opener):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_LOGSTAT`         | 10    | GET/SET  | Syslog status flags (as per `openlog()` option argument). |
| `SBTC_LOGTAGPTR`       | 11    | GET/SET  | Pointer to the syslog identification string (STRPTR). |
| `SBTC_LOGFACILITY`     | 12    | GET/SET  | Syslog facility code. |
| `SBTC_LOGMASK`         | 13    | GET/SET  | Syslog priority mask (as per `setlogmask()`). |

**Error string lookup** (GET only):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_ERRNOSTRPTR`     | 14    | GET      | Maps a ULONG errno value to its string description (STRPTR). Pass the error number in `ti_Data`; on return, `ti_Data` contains a pointer to the string. |
| `SBTC_HERRNOSTRPTR`    | 15    | GET      | Maps an h_errno value to its string description. Same convention as `SBTC_ERRNOSTRPTR`. |
| `SBTC_IOERRNOSTRPTR`   | 16    | GET      | Maps an Exec I/O error code to its string. Note: Exec error codes (from `<exec/errors.h>`) are negative and must be negated before passing. |
| `SBTC_S2ERRNOSTRPTR`   | 17    | GET      | Maps a SANA-II error code to its string. |
| `SBTC_S2WERRNOSTRPTR`  | 18    | GET      | Maps a SANA-II wire error code to its string. |

**errno pointer registration** (SET only):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_ERRNOBYTEPTR`    | 21    | SET      | Register a 1-byte (BYTE) errno pointer. The library writes the low byte of each error code to this address. |
| `SBTC_ERRNOWORDPTR`    | 22    | SET      | Register a 2-byte (WORD) errno pointer. The library writes the low word of each error code. |
| `SBTC_ERRNOLONGPTR`    | 24    | SET      | Register a 4-byte (LONG) errno pointer. The library writes the full 32-bit error code. This is the standard and recommended form. |
| `SBTC_HERRNOLONGPTR`   | 25    | SET      | Register a 4-byte (LONG) h_errno pointer. |

The convenience macro `SBTC_ERRNOPTR(size)` selects the appropriate tag
code based on the size argument: `sizeof(long)` maps to
`SBTC_ERRNOLONGPTR`, `sizeof(short)` to `SBTC_ERRNOWORDPTR`,
`sizeof(char)` to `SBTC_ERRNOBYTEPTR`. An invalid size yields 0, which
causes `SocketBaseTagList()` to fail.

**Release information** (GET only):

| Code                    | Value | Access   | Description                        |
|-------------------------|-------|----------|------------------------------------|
| `SBTC_RELEASESTRPTR`   | 29    | GET      | Pointer to a string identifying the TCP/IP stack version (e.g., "Roadshow 4.364"). |

**Roadshow extensions** (codes 40+, may not be available on all stacks):

| Code                                   | Value | Access   | Description                     |
|-----------------------------------------|-------|----------|---------------------------------|
| `SBTC_NUM_PACKET_FILTER_CHANNELS`      | 40    | GET      | Number of BPF channels available. |
| `SBTC_HAVE_ROUTING_API`                | 41    | GET      | Whether the routing API is supported (BOOL). |
| `SBTC_UDP_CHECKSUM`                    | 42    | GET/SET  | Enable/disable UDP checksums. |
| `SBTC_IP_FORWARDING`                   | 43    | GET/SET  | Enable/disable IP packet forwarding. |
| `SBTC_IP_DEFAULT_TTL`                  | 44    | GET/SET  | Default IP packet TTL value. |
| `SBTC_ICMP_MASK_REPLY`                 | 45    | GET/SET  | Respond to ICMP mask requests. |
| `SBTC_ICMP_SEND_REDIRECTS`             | 46    | GET/SET  | Enable/disable ICMP redirect messages. |
| `SBTC_HAVE_INTERFACE_API`              | 47    | GET      | Whether the interface management API is supported. |
| `SBTC_ICMP_PROCESS_ECHO`              | 48    | GET/SET  | ICMP echo processing mode: `IR_Process` (0), `IR_Ignore` (1), or `IR_Drop` (2). |
| `SBTC_ICMP_PROCESS_TSTAMP`            | 49    | GET/SET  | ICMP timestamp processing mode (same values as echo). |
| `SBTC_HAVE_MONITORING_API`             | 50    | GET      | Whether the monitoring hook API is supported. |
| `SBTC_CAN_SHARE_LIBRARY_BASES`         | 51    | GET      | Whether library bases can be shared between callers. |
| `SBTC_LOG_FILE_NAME`                   | 52    | GET/SET  | Log output file name (STRPTR). |
| `SBTC_HAVE_STATUS_API`                 | 53    | GET      | Whether the `GetNetworkStatistics()` API is supported. |
| `SBTC_HAVE_DNS_API`                    | 54    | GET      | Whether the DNS management API is supported. |
| `SBTC_LOG_HOOK`                        | 55    | GET/SET  | Pointer to log hook (`struct Hook *`). |
| `SBTC_SYSTEM_STATUS`                   | 56    | GET      | System status bitmask: `SBSYSSTAT_Interfaces` (bit 0), `SBSYSSTAT_PTP_Interfaces` (bit 1), `SBSYSSTAT_BCast_Interfaces` (bit 2), `SBSYSSTAT_Resolver` (bit 3), `SBSYSSTAT_Routes` (bit 4), `SBSYSSTAT_DefaultRoute` (bit 5). |
| `SBTC_SIG_ADDRESS_CHANGE_MASK`         | 57    | GET/SET  | Signal mask for interface address change notification. |
| `SBTC_IPF_API_VERSION`                 | 58    | GET      | IP filter API version, if supported. |
| `SBTC_HAVE_LOCAL_DATABASE_API`         | 59    | GET      | Whether the local database (netent/servent/protoent) API is supported. |
| `SBTC_HAVE_ADDRESS_CONVERSION_API`     | 60    | GET      | Whether `inet_aton()`/`inet_ntop()`/`inet_pton()` are supported. |
| `SBTC_HAVE_KERNEL_MEMORY_API`          | 61    | GET      | Whether the mbuf API is supported. |
| `SBTC_IP_FILTER_HOOK`                  | 62    | GET/SET  | IP filter hook (`struct Hook *`). |
| `SBTC_HAVE_SERVER_API`                 | 63    | GET      | Whether `ProcessIsServer()`/`ObtainServerSocket()` are supported. |
| `SBTC_GET_BYTES_RECEIVED`              | 64    | GET      | Total bytes received (SBQUAD_T via GETREF). |
| `SBTC_GET_BYTES_SENT`                  | 65    | GET      | Total bytes sent (SBQUAD_T via GETREF). |
| `SBTC_IDN_DEFAULT_CHARACTER_SET`       | 66    | GET/SET  | IDN character set: `IDNCS_ASCII` (0) or `IDNCS_ISO_8859_LATIN_1` (1). |
| `SBTC_HAVE_ROADSHOWDATA_API`          | 67    | GET      | Whether the RoadshowData API is supported. |
| `SBTC_ERROR_HOOK`                      | 68    | GET/SET  | Error code callback hook (`struct Hook *`). The hook receives an `ErrorHookMsg` with `ehm_Action` set to `EHMA_Set_errno` (1) or `EHMA_Set_h_errno` (2). |
| `SBTC_HAVE_GETHOSTADDR_R_API`         | 69    | GET      | Whether `gethostbyname_r()`/`gethostbyaddr_r()` are supported. |

**Parameters:**

- `tags` --- Pointer to a `struct TagItem` array, terminated by
  `TAG_DONE`. Each tag item contains:
  - `ti_Tag` --- The encoded tag value (constructed with `SBTM_GETVAL`,
    `SBTM_GETREF`, `SBTM_SETVAL`, or `SBTM_SETREF`).
  - `ti_Data` --- Either the value to set (for `SBTM_SETVAL`), a
    pointer to the value to set (for `SBTM_SETREF`), a location to
    receive the value (for `SBTM_GETREF`), or unused on input and
    overwritten with the value on return (for `SBTM_GETVAL`).

**Return Value:**

Returns 0 on success (all tags processed without error). Returns a
positive value indicating the number of tags that could not be
processed. A return value greater than 0 indicates partial failure ---
some tags may have been processed successfully before the error.

**Errors:**

`SocketBaseTagList()` does not set errno. Errors are indicated by the
return value. A tag with an unrecognized code or an invalid operation
(e.g., SET on a GET-only code) increments the error count.

**Notes:**

- The variadic `SocketBaseTags()` form is a macro, not a separate
  library function. It shares LVO -294 with `SocketBaseTagList()`.
  The macro constructs the tag array on the stack, so extremely long
  tag lists may consume significant stack space.

- For `SBTM_GETVAL`, the current value is written back to the
  `ti_Data` field of the `TagItem` structure. When using the variadic
  `SocketBaseTags()` macro, this modification is not visible to the
  caller because the tag array is temporary (on the stack). Therefore,
  `SBTM_GETREF` is the correct choice for reading values through
  `SocketBaseTags()`:

  ```c
  ULONG mask = 0;
  SocketBaseTags(SBTM_GETREF(SBTC_BREAKMASK), (ULONG)&mask, TAG_DONE);
  /* 'mask' now contains the current break signal mask */
  ```

  `SBTM_GETVAL` is useful only when constructing a `TagItem` array
  manually and inspecting `ti_Data` after the call.

- Error string tags (`SBTC_ERRNOSTRPTR`, `SBTC_HERRNOSTRPTR`,
  `SBTC_IOERRNOSTRPTR`, `SBTC_S2ERRNOSTRPTR`, `SBTC_S2WERRNOSTRPTR`)
  use a special convention: pass the error number in `ti_Data`, and on
  return `ti_Data` contains a pointer to the string. Use `SBTM_GETREF`
  with a ULONG variable initialized to the error number:

  ```c
  ULONG val = ECONNREFUSED;  /* error number to look up */
  SocketBaseTags(SBTM_GETREF(SBTC_ERRNOSTRPTR), (ULONG)&val, TAG_DONE);
  /* val now contains a pointer to "Connection refused" or similar */
  printf("Error: %s\n", (STRPTR)val);
  ```

- The `SBTC_ERRNOBYTEPTR`, `SBTC_ERRNOWORDPTR`, and
  `SBTC_ERRNOLONGPTR` tags are SET-only. There is no standard way to
  read back the currently registered errno pointer. Roadshow provides
  GET access for `SBTC_ERRNOLONGPTR` and `SBTC_HERRNOLONGPTR` as an
  extension, but this behavior is not guaranteed across all stacks.
  The bsdsocktest suite tests this as a known failure on stacks that
  do not support it.

- Multiple tags can be combined in a single call:

  ```c
  SocketBaseTags(
      SBTM_SETVAL(SBTC_ERRNOLONGPTR), (ULONG)&bsd_errno,
      SBTM_SETVAL(SBTC_HERRNOLONGPTR), (ULONG)&bsd_h_errno,
      SBTM_SETVAL(SBTC_BREAKMASK), SIGBREAKF_CTRL_C,
      TAG_DONE
  );
  ```

- The Roadshow extension codes (40 and above) are not present in the
  original AmiTCP SDK and may not be supported by all implementations.
  Query capability codes (those beginning with `SBTC_HAVE_`) before
  using the corresponding APIs.

- `SocketBaseTagList()` supersedes `SetSocketSignals()` for signal mask
  configuration and `SetErrnoPtr()` for errno pointer registration. The
  tag interface supports both reading and writing, whereas the legacy
  functions are write-only.

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists. This is an
Amiga-specific mechanism that uses the AmigaOS `TagItem` convention
(from `<utility/tagitem.h>`) for extensible configuration.

**See Also:**

`SetSocketSignals()`, `SetErrnoPtr()`, `Errno()`, `GetSocketEvents()`,
`SO_EVENTMASK`, `SBTC_BREAKMASK`, `SBTC_SIGEVENTMASK`,
`SBTC_ERRNOLONGPTR`, `getdtablesize()`


## Error Handling

The bsdsocket.library maintains a per-opener errno value that is
separate from the C library's `errno`. The `Errno()` function retrieves
this value, and `SetErrnoPtr()` registers a caller-supplied variable
where the library writes error codes automatically.

---

### Errno()

**Library:** bsdsocket.library
**LVO:** -162
**Registers:** (none)

**Synopsis:**

```c
#include <proto/bsdsocket.h>

LONG Errno(void);
```

**Description:**

`Errno()` returns the error code from the most recent failed
bsdsocket.library operation on the calling task's library context.
It is the library-call equivalent of reading the BSD `errno` global
variable.

The value returned is one of the standard BSD error codes defined in
`<sys/errno.h>` (e.g., `EBADF`, `EINVAL`, `ECONNREFUSED`,
`EWOULDBLOCK`). The error code is set by any library function that
reports failure through its return value (typically -1 or NULL).

`Errno()` is the simplest way to retrieve the last socket error, but
it has a significant limitation: the caller cannot read the errno value
after intervening library calls, because each call may overwrite it.
For robust error handling, register an errno pointer via
`SocketBaseTags()` with `SBTC_ERRNOLONGPTR`, which causes the library
to write the error code to a caller-supplied variable after every
operation.

**Parameters:**

(none)

**Return Value:**

Returns the current bsdsocket errno value as a LONG. If no error has
occurred since the library was opened (or since the last successful
operation, on implementations that clear errno on success), returns 0.

**Errors:**

This function does not fail and does not modify the errno value.

**Notes:**

- The BSD specification does not require errno to be cleared on
  success. Some implementations (including Roadshow) may leave the
  previous error code intact after a successful operation. Others may
  clear it. Callers should check the return value of the primary
  operation first and only consult `Errno()` when the return value
  indicates failure.

- `Errno()` reads the same internal per-opener errno value that is
  written to the pointer registered via `SBTC_ERRNOLONGPTR`. If a
  pointer has been registered, reading `Errno()` and reading the
  pointed-to variable will return the same value.

- The `Errno()` function and the registered errno pointer both reflect
  the bsdsocket.library errno, which is entirely separate from the C
  library `errno` (libnix, ixemul, or clib2).

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to reading the BSD `errno`
global variable. Named as a function because AmigaOS shared libraries
cannot export global variables --- only function entry points.

**See Also:**

`SetErrnoPtr()`, `SocketBaseTagList()`, `SBTC_ERRNO`,
`SBTC_ERRNOLONGPTR`, `SBTC_ERRNOSTRPTR`

---

### SetErrnoPtr()

**Library:** bsdsocket.library
**LVO:** -168
**Registers:** A0/D0

**Synopsis:**

```c
#include <proto/bsdsocket.h>

VOID SetErrnoPtr(APTR errno_ptr, LONG size);
```

**Description:**

`SetErrnoPtr()` registers a pointer to a caller-supplied variable where
the bsdsocket.library will write the error code after each operation.
The `size` parameter specifies the width of the target variable, allowing
the library to write to BYTE (1), WORD (2), or LONG (4) storage.

After this call, every bsdsocket.library function that sets errno will
write the error code to the address specified by `errno_ptr`, truncating
to the specified size. This continues until the library is closed or
`SetErrnoPtr()` is called again with a different pointer.

This function is **superseded** by the tag-based interface.
`SocketBaseTags()` with `SBTC_ERRNOLONGPTR` (or `SBTC_ERRNOWORDPTR` or
`SBTC_ERRNOBYTEPTR`) provides the same functionality and is preferred
because it can be combined with other configuration tags in a single
call. `SetErrnoPtr()` remains functional in all conforming
implementations.

**Parameters:**

- `errno_ptr` --- Pointer to the variable where error codes should be
  written. The variable must remain valid (not go out of scope or be
  freed) for the lifetime of the library context. Passing NULL is
  undefined behavior.

- `size` --- Size of the target variable in bytes. Must be one of:
  - 1 --- BYTE. Only the low 8 bits of the error code are written.
  - 2 --- WORD. The low 16 bits are written.
  - 4 --- LONG. The full 32-bit error code is written. This is the
    recommended size.

**Return Value:**

None (VOID).

**Errors:**

This function does not fail and does not set errno. Passing an invalid
size (not 1, 2, or 4) produces undefined behavior.

**Notes:**

- The registered pointer is per-opener state. It affects only the
  calling task's bsdsocket.library context.

- After calling `SetErrnoPtr()` with a local variable for testing
  purposes, callers must restore the original errno pointer before the
  variable goes out of scope. Failure to do so causes the library to
  write to a dangling pointer, corrupting memory:

  ```c
  /* Test with a byte-sized variable */
  BYTE err_byte = 0;
  SetErrnoPtr(&err_byte, 1);
  CloseSocket(-1);  /* Triggers an error */
  /* err_byte now contains EBADF (truncated to byte) */

  /* IMPORTANT: restore the original LONG pointer */
  SetErrnoPtr(&bsd_errno, sizeof(bsd_errno));
  ```

- The tag equivalents are:
  - `SBTM_SETVAL(SBTC_ERRNOBYTEPTR)` for size 1
  - `SBTM_SETVAL(SBTC_ERRNOWORDPTR)` for size 2
  - `SBTM_SETVAL(SBTC_ERRNOLONGPTR)` for size 4

- `SetErrnoPtr()` and the `SBTC_ERRNO*PTR` tags control the same
  internal pointer. Calling one overwrites the effect of the other.

- The `SBTC_ERRNOPTR(size)` macro can be used to select the correct
  tag code at compile time based on a size constant.

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists. Superseded by
`SocketBaseTags()` with `SBTC_ERRNOLONGPTR`, `SBTC_ERRNOWORDPTR`, or
`SBTC_ERRNOBYTEPTR`.

**See Also:**

`Errno()`, `SocketBaseTagList()`, `SBTC_ERRNOLONGPTR`,
`SBTC_ERRNOWORDPTR`, `SBTC_ERRNOBYTEPTR`, `SBTC_ERRNOPTR()`


## I/O Control

The bsdsocket.library provides `IoctlSocket()` as the Amiga replacement
for the BSD `ioctl()` system call when applied to socket descriptors.
It controls per-socket I/O modes such as non-blocking operation, pending
data queries, and asynchronous notification.

---

### IoctlSocket()

**Library:** bsdsocket.library
**LVO:** -114
**Registers:** D0/D1/A0

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <sys/filio.h>

LONG IoctlSocket(LONG sock, ULONG req, APTR argp);
```

**Description:**

`IoctlSocket()` performs control operations on a socket descriptor. It
is the Amiga-specific replacement for the BSD `ioctl()` system call,
named differently because Amiga shared libraries cannot override C
library functions. The operation is selected by the `req` parameter,
and the `argp` parameter points to operation-specific data.

The function supports the following request codes, defined in
`<sys/filio.h>`:

#### FIONBIO --- Set/Clear Non-Blocking I/O

```c
#define FIONBIO  _IOW('f', 126, __LONG)  /* 0x8004667E */
```

Sets or clears non-blocking mode on the socket. `argp` points to a
LONG value: non-zero enables non-blocking mode, zero disables it.

In non-blocking mode, operations that would normally block (such as
`recv()` on a socket with no data, or `connect()` on a TCP socket)
return immediately with errno set to `EWOULDBLOCK` or `EINPROGRESS`
instead of waiting.

```c
LONG one = 1;
IoctlSocket(sock, FIONBIO, (APTR)&one);    /* enable */

LONG zero = 0;
IoctlSocket(sock, FIONBIO, (APTR)&zero);   /* disable */
```

#### FIONREAD --- Query Pending Bytes

```c
#define FIONREAD  _IOR('f', 127, __LONG)  /* 0x4004667F */
```

Returns the number of bytes available to be read from the socket without
blocking. `argp` points to a LONG that receives the byte count.

For TCP (SOCK_STREAM) sockets, this is the number of bytes in the
receive buffer. For UDP (SOCK_DGRAM) sockets, this is the size of the
next pending datagram.

```c
LONG pending = 0;
IoctlSocket(sock, FIONREAD, (APTR)&pending);
/* pending now contains the number of bytes available */
```

#### FIOASYNC --- Set/Clear Async I/O

```c
#define FIOASYNC  _IOW('f', 125, __LONG)  /* 0x8004667D */
```

Enables or disables asynchronous I/O notification for the socket.
`argp` points to a LONG value: non-zero enables async mode, zero
disables it.

When async mode is enabled and a signal mask has been registered via
`SBTC_SIGIOMASK` (or `SetSocketSignals()`), the library delivers
the I/O signal when data arrives or send buffer space becomes available
on this socket.

```c
LONG one = 1;
IoctlSocket(sock, FIOASYNC, (APTR)&one);   /* enable */
```

Note: `FIOASYNC` may not be supported on all implementations. If not
supported, `IoctlSocket()` returns -1.

#### Other Request Codes

The `<sys/filio.h>` header also defines `FIOSETOWN`, `FIOGETOWN`,
`FIOCLEX`, and `FIONCLEX`. These are not commonly used with
bsdsocket.library sockets and may not be supported by all
implementations. They are not tested by the bsdsocktest suite.

**Parameters:**

- `sock` --- The socket descriptor to operate on.
- `req` --- The ioctl request code. Must be one of `FIONBIO`,
  `FIONREAD`, or `FIOASYNC` (other codes may be supported by specific
  implementations).
- `argp` --- Pointer to operation-specific data. For `FIONBIO` and
  `FIOASYNC`, points to a LONG containing the enable/disable flag.
  For `FIONREAD`, points to a LONG that receives the byte count.

**Return Value:**

Returns 0 on success. Returns -1 on failure and sets the bsdsocket
errno.

**Errors:**

- `EBADF` --- `sock` is not a valid open socket descriptor.
- `EINVAL` --- `req` is not a recognized request code, or `argp` is
  invalid.

**Notes:**

- `FIONBIO` is the standard mechanism for enabling non-blocking mode
  on Amiga sockets. There is no `fcntl()` equivalent in the
  bsdsocket.library API. The `O_NONBLOCK` flag and `fcntl(F_SETFL)`
  approach used on POSIX systems is not available.

- `FIONREAD` returns the exact number of bytes available in the receive
  buffer. After sending 100 bytes to a connected peer and waiting for
  delivery, `FIONREAD` on the receiving socket returns 100.

- The ioctl request codes use the BSD `_IOW`/`_IOR` macro encoding
  from `<sys/ioccom.h>`. The high bits encode direction (in/out) and
  parameter size; the low bits encode the group character ('f' for
  file operations) and the command number.

- The `APTR` type for `argp` in the prototype means the caller must
  cast the pointer explicitly when using the inline stubs:

  ```c
  LONG val = 1;
  IoctlSocket(sock, FIONBIO, (APTR)&val);
  ```

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to the BSD `ioctl()` system
call when applied to socket descriptors, restricted to the `FIO*`
request codes. Named `IoctlSocket()` rather than `ioctl()` because
Amiga shared libraries cannot override C library functions.

**See Also:**

`setsockopt()`, `getsockopt()`, `WaitSelect()`,
`SetSocketSignals()`, `SBTC_SIGIOMASK`,
[ioctl(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=ioctl&sektion=2)


---


## Descriptor Transfer

The bsdsocket.library provides three functions for transferring socket
descriptors between contexts: `Dup2Socket()` duplicates a descriptor
within the same opener context, while `ReleaseSocket()` /
`ReleaseCopyOfSocket()` / `ObtainSocket()` implement a shared pool
mechanism for passing sockets between processes (or between the same
process and the inetd super-server).

The descriptor transfer mechanism is designed for the AmigaOS
multi-process model. Because each task opens bsdsocket.library
independently and has its own descriptor table, there is no POSIX-style
file descriptor inheritance across `fork()`. Instead, a process releases
a socket into a global pool with an identifying key, and another process
obtains it from the pool using that key.

---

### Dup2Socket()

**Library:** bsdsocket.library
**LVO:** -264
**Registers:** D0/D1

**Synopsis:**

```c
#include <proto/bsdsocket.h>

LONG Dup2Socket(LONG old_socket, LONG new_socket);
```

**Description:**

`Dup2Socket()` duplicates a socket descriptor within the calling task's
descriptor table. It operates in two modes depending on the value of
`new_socket`:

- **Dup mode** (`new_socket` = -1): Allocates the lowest available
  descriptor number and creates a duplicate of `old_socket` at that
  slot. This is equivalent to the POSIX `dup()` system call.

- **Dup2 mode** (`new_socket` >= 0): Creates a duplicate of
  `old_socket` at the specified descriptor number `new_socket`. If
  `new_socket` is already in use, the existing socket at that
  descriptor is closed first. This is equivalent to the POSIX `dup2()`
  system call.

After a successful duplication, both descriptors refer to the same
underlying socket. Data can be sent or received through either
descriptor. Closing one descriptor does not affect the other --- the
underlying socket remains open until all descriptors referring to it
are closed.

**Parameters:**

- `old_socket` --- The socket descriptor to duplicate. Must be a valid
  descriptor in the caller's descriptor table.

- `new_socket` --- The target descriptor number, or -1 to request the
  lowest available descriptor.
  - -1: Dup mode. The library assigns the lowest available descriptor.
  - >= 0: Dup2 mode. The duplicate is created at this specific
    descriptor number. If the number is already in use, the existing
    socket is closed first.

**Return Value:**

Returns the new descriptor number on success (which equals `new_socket`
when `new_socket` >= 0, or the lowest available descriptor when
`new_socket` = -1). Returns -1 on failure and sets the bsdsocket errno.

**Errors:**

- `EBADF` --- `old_socket` is not a valid open socket descriptor.

**Notes:**

- Both the original and the duplicated descriptor share the same
  underlying socket. Receiving data through one descriptor consumes it
  for both. The bsdsocktest suite verifies this by sending data to a
  connected socket, then receiving it through the duplicated descriptor
  (test 117).

- In dup mode (`new_socket` = -1), the returned descriptor is always
  different from `old_socket`. The test suite verifies that the
  returned value is non-negative and distinct from the original (test
  115).

- In dup2 mode (`new_socket` >= 0), the test suite requests a target
  descriptor 10 slots above the original to avoid collisions (test
  116). The function either returns the requested target or -1 if the
  target is out of range.

- `Dup2Socket()` operates within the caller's per-opener descriptor
  table. It cannot transfer sockets to other processes. For
  inter-process socket transfer, use `ReleaseSocket()` /
  `ObtainSocket()`.

**Conformance:**

AmiTCP SDK 4.0. Combines the semantics of POSIX `dup()` (when
`new_socket` = -1) and POSIX `dup2()` (when `new_socket` >= 0) into
a single function. Named `Dup2Socket()` rather than `dup2()` because
Amiga shared libraries cannot override C library functions.

**See Also:**

`CloseSocket()`, `ReleaseSocket()`, `ObtainSocket()`,
[dup(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=dup&sektion=2),
[dup2(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=dup2&sektion=2)

---

### ObtainSocket()

**Library:** bsdsocket.library
**LVO:** -144
**Registers:** D0/D1/D2/D3

**Synopsis:**

```c
#include <proto/bsdsocket.h>

LONG ObtainSocket(LONG id, LONG domain, LONG type, LONG protocol);
```

**Description:**

`ObtainSocket()` retrieves a socket from the bsdsocket.library's global
shared socket pool. The socket must have been previously placed into the
pool by a call to `ReleaseSocket()` or `ReleaseCopyOfSocket()` (from
the same or a different process).

The `id` parameter is the key used to locate the socket in the pool. It
must match the `id` returned by the `ReleaseSocket()` or
`ReleaseCopyOfSocket()` call that placed the socket into the pool.

The `domain`, `type`, and `protocol` parameters must match the
properties of the released socket. If they do not match, the function
fails. These parameters serve as a safety check to ensure that the
caller is obtaining the expected type of socket.

On success, the socket is removed from the shared pool and inserted
into the caller's per-opener descriptor table. The returned descriptor
number is valid within the caller's context and can be used for all
subsequent socket operations.

This mechanism is the Amiga equivalent of the POSIX practice of
inheriting socket descriptors across `fork()`/`exec()`. It is used
by the AmiTCP inetd super-server to pass accepted connections to
service handler processes.

**Parameters:**

- `id` --- The identifier key of the socket to obtain. This is the
  value returned by `ReleaseSocket()` or `ReleaseCopyOfSocket()`.
  Valid id values depend on the id passed to the release function:
  - If the release used a specific non-negative id, that same value
    is returned and must be used here.
  - If the release used `UNIQUE_ID` (-1), the library assigned a
    unique id, which was returned by the release call and must be
    used here.

- `domain` --- The protocol family of the socket. Must match the
  domain used when the socket was created (typically `AF_INET`).

- `type` --- The socket type. Must match the type used when the socket
  was created (e.g., `SOCK_STREAM`, `SOCK_DGRAM`).

- `protocol` --- The protocol. Must match the protocol used when the
  socket was created (typically 0 or `IPPROTO_TCP`).

**Return Value:**

Returns the new socket descriptor number on success (a non-negative
LONG). Returns -1 on failure and sets the bsdsocket errno.

**Errors:**

- `EINVAL` --- No socket with the specified `id` exists in the shared
  pool, or the `domain`/`type`/`protocol` do not match the released
  socket's properties.

**Notes:**

- `ObtainSocket()` removes the socket from the shared pool. A given
  socket can only be obtained once. After `ObtainSocket()` succeeds,
  the `id` is no longer valid in the pool.

- The bsdsocktest suite tests the `ReleaseSocket()` /
  `ObtainSocket()` round-trip within a single process (test 118). It
  sends data before releasing, then receives it after obtaining, which
  verifies that pending data survives the transfer.

- In the typical inetd usage pattern, a `struct DaemonMessage` is
  passed to the server process via the AmigaOS `pr_ExitData` field.
  The message contains the `dm_ID`, `dm_Family`, and `dm_Type` fields
  that are passed to `ObtainSocket()`. See the `DaemonMessage`
  structure in `<libraries/bsdsocket.h>` for details.

- Passing `protocol` as 0 matches any protocol within the specified
  domain and type. This is the common usage.

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists. This is an
Amiga-specific mechanism for inter-process socket transfer, replacing
the POSIX pattern of socket inheritance via `fork()`.

**See Also:**

`ReleaseSocket()`, `ReleaseCopyOfSocket()`, `Dup2Socket()`,
`UNIQUE_ID`, `struct DaemonMessage`

---

### ReleaseSocket()

**Library:** bsdsocket.library
**LVO:** -150
**Registers:** D0/D1

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <libraries/bsdsocket.h>

LONG ReleaseSocket(LONG sock, LONG id);
```

**Description:**

`ReleaseSocket()` removes a socket from the caller's per-opener
descriptor table and places it into the bsdsocket.library's global
shared socket pool, where it can be retrieved by another process (or
the same process) using `ObtainSocket()`.

After a successful call, the descriptor `sock` is no longer valid in
the caller's context. The caller must not use it for any further
operations --- it is as if `CloseSocket()` had been called, except
that the underlying socket is not destroyed but rather transferred
to the pool.

The `id` parameter assigns an identifier key to the released socket.
This key is used by the process calling `ObtainSocket()` to locate the
socket in the pool.

**Parameters:**

- `sock` --- The socket descriptor to release. Must be a valid
  descriptor in the caller's descriptor table.

- `id` --- The identifier key to assign to the released socket.
  - A non-negative value (0 through 65535 by convention): Used as the
    key directly. The caller and the process calling `ObtainSocket()`
    must agree on this value out of band.
  - `UNIQUE_ID` (-1): The library assigns a unique identifier
    automatically and returns it. This avoids key collisions when
    multiple sockets are released concurrently.

**Return Value:**

Returns the effective identifier key on success. If `id` was a
non-negative value, the return value equals `id`. If `id` was
`UNIQUE_ID`, the return value is the library-assigned unique key.
Returns -1 on failure and sets the bsdsocket errno.

**Errors:**

- `EBADF` --- `sock` is not a valid open socket descriptor.

**Notes:**

- After `ReleaseSocket()` succeeds, the descriptor `sock` is invalid.
  The test suite sets `server = -1` immediately after a successful
  release to prevent accidental use of the stale descriptor (test 118).

- `UNIQUE_ID` is defined as -1 in `<libraries/bsdsocket.h>`. Using
  `UNIQUE_ID` is the safest approach for avoiding pool key collisions
  between unrelated programs.

- The shared pool persists for the lifetime of the TCP/IP stack (not
  the lifetime of the caller's library context). A socket released by
  one process can be obtained by another process that opens the library
  independently.

- Unlike `ReleaseCopyOfSocket()`, `ReleaseSocket()` transfers
  ownership. The original descriptor is invalidated. If the caller
  needs to retain access to the socket while also making it available
  in the pool, use `ReleaseCopyOfSocket()` instead.

- If no process calls `ObtainSocket()` to retrieve a released socket,
  the socket remains in the pool and is eventually cleaned up when
  the TCP/IP stack shuts down.

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists. This is an
Amiga-specific mechanism for inter-process socket transfer.

**See Also:**

`ObtainSocket()`, `ReleaseCopyOfSocket()`, `Dup2Socket()`,
`CloseSocket()`, `UNIQUE_ID`

---

### ReleaseCopyOfSocket()

**Library:** bsdsocket.library
**LVO:** -156
**Registers:** D0/D1

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <libraries/bsdsocket.h>

LONG ReleaseCopyOfSocket(LONG sock, LONG id);
```

**Description:**

`ReleaseCopyOfSocket()` places a copy of the socket into the
bsdsocket.library's global shared socket pool while retaining the
original descriptor in the caller's descriptor table. This allows the
caller to continue using the socket while simultaneously making it
available for another process to obtain via `ObtainSocket()`.

This function differs from `ReleaseSocket()` in one critical respect:
the original descriptor `sock` remains valid and usable after the call.
With `ReleaseSocket()`, the original descriptor is invalidated.

The `id` parameter works identically to `ReleaseSocket()` --- it
assigns a key to the pooled socket for retrieval by `ObtainSocket()`.

**Parameters:**

- `sock` --- The socket descriptor to copy into the pool. Must be a
  valid descriptor in the caller's descriptor table. The descriptor
  remains valid after the call.

- `id` --- The identifier key to assign to the released copy.
  - A non-negative value: Used as the key directly.
  - `UNIQUE_ID` (-1): The library assigns a unique identifier
    automatically and returns it.

**Return Value:**

Returns the effective identifier key on success. If `id` was a
non-negative value, the return value equals `id`. If `id` was
`UNIQUE_ID`, the return value is the library-assigned unique key.
Returns -1 on failure and sets the bsdsocket errno.

**Errors:**

- `EBADF` --- `sock` is not a valid open socket descriptor.

**Notes:**

- The key difference from `ReleaseSocket()` is that the original
  descriptor remains valid. The bsdsocktest suite verifies this by
  sending data after `ReleaseCopyOfSocket()` and receiving it through
  the original descriptor (test 119). The test confirms that the
  original socket is fully functional after the copy is released.

- Both the original descriptor and the pooled copy refer to the same
  underlying socket. Data received on one is consumed for both. The
  behavior when both the original and the obtained copy are used
  concurrently for I/O is implementation-defined and should be
  avoided.

- The copy placed in the pool is abandoned if no process calls
  `ObtainSocket()` to retrieve it. The test suite's comment notes that
  the abandoned copy is "cleaned up at library close."

- `UNIQUE_ID` is defined as -1 in `<libraries/bsdsocket.h>`.

**Conformance:**

AmiTCP SDK 4.0. No POSIX or BSD equivalent exists. This is an
Amiga-specific mechanism.

**See Also:**

`ReleaseSocket()`, `ObtainSocket()`, `Dup2Socket()`, `UNIQUE_ID`


## Address Utilities

The bsdsocket.library provides several functions for converting between
IPv4 address representations. These are Amiga-named variants of the
classic BSD `inet_ntoa()`, `inet_lnaof()`, `inet_netof()`, and
`inet_makeaddr()` functions. They use classful network addressing
(Class A, B, C) to decompose and reconstruct IP addresses.

The Amiga variants take and return `in_addr_t` (a 32-bit unsigned
integer in network byte order) rather than `struct in_addr`. On the
Amiga (68k, big-endian), network byte order and host byte order are
the same, so the `htonl()`/`ntohl()` macros are identity operations.

---

### Inet_NtoA()

**Library:** bsdsocket.library
**LVO:** -174
**Registers:** D0

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <netinet/in.h>

STRPTR Inet_NtoA(in_addr_t ip);
```

**Description:**

`Inet_NtoA()` converts a 32-bit IPv4 address in network byte order
to its dotted-decimal string representation (e.g., "127.0.0.1").

The function returns a pointer to a statically allocated buffer
within the library. The buffer is overwritten by subsequent calls to
`Inet_NtoA()` within the same opener context. The caller must copy the
string if it needs to persist beyond the next call.

This is the Amiga variant of the BSD `inet_ntoa()` function. The key
differences are:

- `Inet_NtoA()` takes an `in_addr_t` (a bare 32-bit integer) rather
  than a `struct in_addr`.
- The function is a library call (LVO -174) rather than a C library
  function. It uses the D0 register for the parameter.

**Parameters:**

- `ip` --- The IPv4 address in network byte order (`in_addr_t`).

**Return Value:**

Returns a pointer to a NUL-terminated string containing the
dotted-decimal representation of the address. The pointer refers to
a static buffer that is overwritten by subsequent calls.

**Errors:**

This function does not fail and does not set errno.

**Notes:**

- The returned string is in a per-opener static buffer. It is safe
  to use until the next call to `Inet_NtoA()` within the same task.

- The bsdsocktest suite tests three cases (tests 105--107):
  - `Inet_NtoA(htonl(0x7f000001))` returns "127.0.0.1"
  - `Inet_NtoA(htonl(0xffffffff))` returns "255.255.255.255"
  - `Inet_NtoA(0)` returns "0.0.0.0"

- Since the 68k Amiga is big-endian, `htonl()` is a no-op. The raw
  value 0 corresponds to 0.0.0.0 without conversion.

- Do not free the returned pointer. It points to library-internal
  storage.

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to the BSD `inet_ntoa(3)`
function, with the parameter type changed from `struct in_addr` to
`in_addr_t`. Named `Inet_NtoA()` because Amiga shared libraries use
CamelCase naming conventions.

**See Also:**

`inet_addr()`, `Inet_MakeAddr()`,
[inet_ntoa(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=inet_ntoa&sektion=3)

---

### Inet_LnaOf()

**Library:** bsdsocket.library
**LVO:** -186
**Registers:** D0

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <netinet/in.h>

in_addr_t Inet_LnaOf(in_addr_t in);
```

**Description:**

`Inet_LnaOf()` extracts the host (local network address) part of an
IPv4 address using classful network addressing rules. The function
examines the high-order bits of the address to determine the address
class, then returns the host portion:

- **Class A** (high bit 0, first octet 0--127): Host part is the low
  24 bits (mask `0x00FFFFFF`).
- **Class B** (high bits 10, first octet 128--191): Host part is the
  low 16 bits (mask `0x0000FFFF`).
- **Class C** (high bits 110, first octet 192--223): Host part is the
  low 8 bits (mask `0x000000FF`).

The returned value is the host number in host byte order (not shifted
or masked into network byte order). For a Class A address like
10.1.2.3 (`0x0A010203` in network byte order), the host part is
`0x010203`.

**Parameters:**

- `in` --- The IPv4 address in network byte order (`in_addr_t`).

**Return Value:**

Returns the host part of the address as an `in_addr_t` in host byte
order.

**Errors:**

This function does not fail and does not set errno.

**Notes:**

- This function uses the historical classful addressing scheme (RFC
  791). It does not consider subnet masks or CIDR prefixes. Classful
  addressing is obsolete for routing purposes but is preserved in the
  BSD API for backward compatibility.

- The bsdsocktest suite tests with the Class A address 10.1.2.3,
  verifying that `Inet_LnaOf(htonl(0x0a010203))` returns `0x010203`
  (test 111).

- `Inet_LnaOf()` is the complement of `Inet_NetOf()`. Given an
  address, `Inet_NetOf()` extracts the network part and `Inet_LnaOf()`
  extracts the host part. `Inet_MakeAddr()` reconstructs the original
  address from these two parts.

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to the BSD `inet_lnaof(3)`
function, with the parameter type changed from `struct in_addr` to
`in_addr_t`.

**See Also:**

`Inet_NetOf()`, `Inet_MakeAddr()`,
[inet_lnaof(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=inet_lnaof&sektion=3)

---

### Inet_NetOf()

**Library:** bsdsocket.library
**LVO:** -192
**Registers:** D0

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <netinet/in.h>

in_addr_t Inet_NetOf(in_addr_t in);
```

**Description:**

`Inet_NetOf()` extracts the network part of an IPv4 address using
classful network addressing rules. The function examines the
high-order bits of the address to determine the address class, then
returns the network number:

- **Class A** (high bit 0, first octet 0--127): Network part is the
  high 8 bits, returned as-is (e.g., 10 for 10.x.x.x).
- **Class B** (high bits 10, first octet 128--191): Network part is
  the high 16 bits.
- **Class C** (high bits 110, first octet 192--223): Network part is
  the high 24 bits.

The returned value is the network number in host byte order.

**Parameters:**

- `in` --- The IPv4 address in network byte order (`in_addr_t`).

**Return Value:**

Returns the network part of the address as an `in_addr_t` in host byte
order.

**Errors:**

This function does not fail and does not set errno.

**Notes:**

- Like `Inet_LnaOf()`, this function uses classful addressing (RFC
  791) and does not consider subnet masks.

- The bsdsocktest suite tests with the Class A address 10.1.2.3,
  verifying that `Inet_NetOf(htonl(0x0a010203))` returns `0x0a` (test
  112).

- `Inet_NetOf()` is the complement of `Inet_LnaOf()`. The values
  returned by both functions can be passed to `Inet_MakeAddr()` to
  reconstruct the original address.

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to the BSD `inet_netof(3)`
function, with the parameter type changed from `struct in_addr` to
`in_addr_t`.

**See Also:**

`Inet_LnaOf()`, `Inet_MakeAddr()`,
[inet_netof(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=inet_netof&sektion=3)

---

### Inet_MakeAddr()

**Library:** bsdsocket.library
**LVO:** -198
**Registers:** D0/D1

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <netinet/in.h>

in_addr_t Inet_MakeAddr(in_addr_t net, in_addr_t host);
```

**Description:**

`Inet_MakeAddr()` constructs a complete IPv4 address from a network
number and a host number, using classful network addressing rules to
determine how the two values are combined.

The function examines the `net` value to determine the address class:

- **Class A** (net < 128): The network number occupies the high 8 bits
  and the host number occupies the low 24 bits.
- **Class B** (net < 65536): The network number occupies the high 16
  bits and the host number occupies the low 16 bits.
- **Class C** (net >= 65536): The network number occupies the high 24
  bits and the host number occupies the low 8 bits.

The returned value is a complete IPv4 address in network byte order.

**Parameters:**

- `net` --- The network number in host byte order, as returned by
  `Inet_NetOf()` or `inet_network()`.

- `host` --- The host number in host byte order, as returned by
  `Inet_LnaOf()`.

**Return Value:**

Returns the constructed IPv4 address as an `in_addr_t` in network byte
order.

**Errors:**

This function does not fail and does not set errno.

**Notes:**

- `Inet_MakeAddr()` is the inverse of the `Inet_NetOf()` /
  `Inet_LnaOf()` decomposition. The bsdsocktest suite verifies the
  round-trip: decomposing 10.1.2.3 into its network and host parts
  with `Inet_NetOf()` and `Inet_LnaOf()`, then reconstructing with
  `Inet_MakeAddr()` and comparing against the original address (test
  113).

- Like its component functions, `Inet_MakeAddr()` uses classful
  addressing (RFC 791) and does not consider subnet masks or CIDR
  prefixes.

- The function determines the class from the `net` parameter's
  magnitude, not from the address bits. This means `net` values
  returned by `Inet_NetOf()` are correctly classified when passed
  back to `Inet_MakeAddr()`.

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to the BSD `inet_makeaddr(3)`
function, with parameter and return types changed from `struct in_addr`
to `in_addr_t`.

**See Also:**

`Inet_NetOf()`, `Inet_LnaOf()`, `inet_addr()`, `inet_network()`,
[inet_makeaddr(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=inet_makeaddr&sektion=3)


## Logging

The bsdsocket.library provides `syslog()` and `vsyslog()` functions
for logging messages through the TCP/IP stack's logging subsystem.
These are modeled after the BSD syslog interface, using the same
priority levels and facility codes.

On AmigaOS, syslog messages are typically written to a log file
managed by the TCP/IP stack, rather than to a syslog daemon over the
network. The log configuration is per-opener state, managed through
`SocketBaseTags()` with the `SBTC_LOG*` tag codes.

---

### vsyslog()

**Library:** bsdsocket.library
**LVO:** -258
**Registers:** D0/A0/A1

**Synopsis:**

```c
#include <proto/bsdsocket.h>
#include <sys/syslog.h>

VOID vsyslog(LONG pri, STRPTR msg, APTR args);
```

**Description:**

`vsyslog()` writes a formatted message to the TCP/IP stack's syslog
facility. The message is formatted using `printf()`-style format
specifiers in `msg`, with the arguments supplied as a pointer to an
array of values in `args`.

The `pri` parameter encodes both the syslog facility and the priority
level. These are combined using bitwise OR. If no facility is specified
(i.e., the facility bits are all zero), the default facility registered
via `SBTC_LOGFACILITY` is used.

`vsyslog()` is the actual library function (LVO -258). The variadic
`syslog()` function is a convenience macro defined in
`<inline/bsdsocket.h>` that constructs the argument array on the
stack and calls `vsyslog()`.

**Parameters:**

- `pri` --- The combined facility and priority value. Constructed by
  ORing a facility code with a priority level:

  Priority levels (defined in `<sys/syslog.h>`):

  | Constant      | Value | Description                        |
  |---------------|-------|------------------------------------|
  | `LOG_EMERG`   | 0     | System is unusable                 |
  | `LOG_ALERT`   | 1     | Action must be taken immediately   |
  | `LOG_CRIT`    | 2     | Critical conditions                |
  | `LOG_ERR`     | 3     | Error conditions                   |
  | `LOG_WARNING` | 4     | Warning conditions                 |
  | `LOG_NOTICE`  | 5     | Normal but significant condition   |
  | `LOG_INFO`    | 6     | Informational                      |
  | `LOG_DEBUG`   | 7     | Debug-level messages               |

  Facility codes (defined in `<sys/syslog.h>`):

  | Constant        | Value   | Description                      |
  |-----------------|---------|----------------------------------|
  | `LOG_KERN`      | 0 << 3  | Kernel messages                  |
  | `LOG_USER`      | 1 << 3  | User-level messages              |
  | `LOG_MAIL`      | 2 << 3  | Mail system                      |
  | `LOG_DAEMON`    | 3 << 3  | System daemons                   |
  | `LOG_AUTH`      | 4 << 3  | Authorization                    |
  | `LOG_SYSLOG`    | 5 << 3  | Internal syslog                  |
  | `LOG_LPR`       | 6 << 3  | Printer subsystem                |
  | `LOG_NEWS`      | 7 << 3  | Network news                     |
  | `LOG_UUCP`      | 8 << 3  | UUCP subsystem                   |
  | `LOG_CRON`      | 9 << 3  | Clock daemon                     |
  | `LOG_AUTHPRIV`  | 10 << 3 | Private authorization            |
  | `LOG_FTP`       | 11 << 3 | FTP daemon                       |
  | `LOG_LOCAL0`--`LOG_LOCAL7` | 16--23 << 3 | Local use 0--7  |

  The priority is in the low 3 bits (`LOG_PRIMASK` = 0x07). The
  facility is in bits 3--9 (`LOG_FACMASK` = 0x03F8).

  Example: `LOG_DAEMON | LOG_INFO` specifies an informational message
  from a daemon.

- `msg` --- A `printf()`-style format string (STRPTR). On AmigaOS,
  this uses the AmigaOS `RawDoFmt()` formatting conventions, which
  differ from ANSI C `printf()` in some respects (e.g., `%ld` for
  LONGs, no floating-point support).

- `args` --- Pointer to an array of arguments matching the format
  string. On 68k AmigaOS, arguments are packed as a contiguous array
  of ULONGs (the AmigaOS varargs convention). Pass NULL if `msg`
  contains no format specifiers.

**Return Value:**

None (VOID).

**Errors:**

This function does not set errno. If the logging subsystem is not
configured or the message cannot be delivered, the call is silently
ignored.

**Notes:**

- The syslog identification tag (the string prepended to each
  message) should be set via `SBTC_LOGTAGPTR` before calling
  `syslog()` or `vsyslog()`:

  ```c
  SocketBaseTags(SBTM_SETVAL(SBTC_LOGTAGPTR),
                 (ULONG)(STRPTR)"myprogram", TAG_DONE);
  ```

- The bsdsocktest suite tests `vsyslog()` directly (test 128) because
  the `syslog()` convenience macro in the SDK headers references
  `_sfdc_vararg`, which is not defined in all SDK configurations. The
  test constructs a ULONG argument array manually and calls `vsyslog()`
  with a format string containing one `%s` specifier. The test is a
  "canary" --- it verifies that the call does not crash, but does not
  verify the log output.

- The `syslog()` convenience macro is defined in
  `<inline/bsdsocket.h>` as:

  ```c
  #define syslog(pri, msg, ...) \
       ({_sfdc_vararg _args[] = { __VA_ARGS__ }; \
        vsyslog((pri), (msg), (const APTR) _args); })
  ```

  This macro may fail to compile if `_sfdc_vararg` is not typedef'd
  in the SDK. In that case, call `vsyslog()` directly with a manually
  constructed argument array.

- The syslog-related `SBTC_*` tag codes allow full configuration of
  the per-opener logging state:
  - `SBTC_LOGSTAT` (10): Option flags (`LOG_PID`, `LOG_CONS`, etc.)
  - `SBTC_LOGTAGPTR` (11): Identification string pointer
  - `SBTC_LOGFACILITY` (12): Default facility code
  - `SBTC_LOGMASK` (13): Priority mask (controls which priorities
    are logged)

- The `LOG_MASK(pri)` macro generates a mask for a single priority
  level: `(1 << (pri))`. The `LOG_UPTO(pri)` macro generates a mask
  for all priorities up to and including `pri`:
  `((1 << ((pri) + 1)) - 1)`.

- The option flags for `SBTC_LOGSTAT` are defined in `<sys/syslog.h>`:
  `LOG_PID` (0x01), `LOG_CONS` (0x02), `LOG_NDELAY` (0x08),
  `LOG_PERROR` (0x20). Not all options may be supported by all
  implementations.

**Conformance:**

AmiTCP SDK 4.0. Functionally equivalent to the BSD `vsyslog(3)` and
`syslog(3)` functions. The underlying library function is `vsyslog()`
(LVO -258); `syslog()` is a variadic convenience macro. The `openlog()`
and `closelog()` BSD functions are not present as library calls --- their
functionality is provided by the `SBTC_LOG*` tags through
`SocketBaseTags()`.

**See Also:**

`SocketBaseTagList()`, `SBTC_LOGTAGPTR`, `SBTC_LOGFACILITY`,
`SBTC_LOGSTAT`, `SBTC_LOGMASK`,
[syslog(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=syslog&sektion=3),
[POSIX syslog()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/syslog.html)


## BSD Function Quick Reference

The following 28 functions are standard BSD socket API functions
available through bsdsocket.library with the same semantics as their
POSIX counterparts. They are provided as library calls rather than
system calls, so they use the 68k register-based calling convention
and require `SocketBase` to be set.

Amiga-specific differences are noted where they exist. For full
documentation, consult the linked FreeBSD man pages and POSIX
specifications.

---

### Socket Creation and Connection

#### socket()

**LVO:** -30 | **Registers:** D0/D1/D2

```c
LONG socket(LONG domain, LONG type, LONG protocol);
```

Creates a communication endpoint. Returns a socket descriptor or -1
on error. The descriptor is managed by bsdsocket.library, not the
AmigaOS file system.

[socket(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=socket&sektion=2) |
[POSIX socket()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/socket.html)

#### bind()

**LVO:** -36 | **Registers:** D0/A0/D1

```c
LONG bind(LONG sock, struct sockaddr *name, socklen_t namelen);
```

Binds a name (address) to a socket. The `name` parameter uses the
BSD 4.4 `sockaddr` with `sa_len` field.

[bind(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=bind&sektion=2) |
[POSIX bind()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/bind.html)

#### listen()

**LVO:** -42 | **Registers:** D0/D1

```c
LONG listen(LONG sock, LONG backlog);
```

Marks a socket as a passive socket that will accept incoming
connections. `SOMAXCONN` is defined as 5 in the AmiTCP SDK headers.

[listen(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=listen&sektion=2) |
[POSIX listen()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/listen.html)

#### accept()

**LVO:** -48 | **Registers:** D0/A0/A1

```c
LONG accept(LONG sock, struct sockaddr *addr, socklen_t *addrlen);
```

Accepts an incoming connection on a listening socket. Returns a new
socket descriptor for the connected peer.

[accept(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=accept&sektion=2) |
[POSIX accept()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/accept.html)

#### connect()

**LVO:** -54 | **Registers:** D0/A0/D1

```c
LONG connect(LONG sock, struct sockaddr *name, socklen_t namelen);
```

Initiates a connection on a socket. For non-blocking sockets, returns
-1 with errno `EINPROGRESS`; use `WaitSelect()` to detect completion.

[connect(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=connect&sektion=2) |
[POSIX connect()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/connect.html)

#### shutdown()

**LVO:** -84 | **Registers:** D0/D1

```c
LONG shutdown(LONG sock, LONG how);
```

Shuts down part of a full-duplex connection. `how`: `SHUT_RD` (0),
`SHUT_WR` (1), or `SHUT_RDWR` (2).

[shutdown(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=shutdown&sektion=2) |
[POSIX shutdown()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/shutdown.html)

---

### Data Transfer

#### send()

**LVO:** -66 | **Registers:** D0/A0/D1/D2

```c
LONG send(LONG sock, APTR buf, LONG len, LONG flags);
```

Sends data on a connected socket. The `buf` parameter is typed as
`APTR` in the library prototype; cast string literals to `(APTR)` or
`(UBYTE *)` when calling through the inline stubs.

[send(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=send&sektion=2) |
[POSIX send()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/send.html)

#### recv()

**LVO:** -78 | **Registers:** D0/A0/D1/D2

```c
LONG recv(LONG sock, APTR buf, LONG len, LONG flags);
```

Receives data from a connected socket. Returns 0 on graceful
connection close (EOF). `MSG_OOB` support is stack-dependent ---
Roadshow documents it as unsupported for `recv()`.

[recv(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=recv&sektion=2) |
[POSIX recv()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/recv.html)

#### sendto()

**LVO:** -60 | **Registers:** D0/A0/D1/D2/A1/D3

```c
LONG sendto(LONG sock, APTR buf, LONG len, LONG flags,
            struct sockaddr *to, socklen_t tolen);
```

Sends data to a specific destination address. Used primarily with
`SOCK_DGRAM` (UDP) sockets.

[sendto(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=sendto&sektion=2) |
[POSIX sendto()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/sendto.html)

#### recvfrom()

**LVO:** -72 | **Registers:** D0/A0/D1/D2/A1/A2

```c
LONG recvfrom(LONG sock, APTR buf, LONG len, LONG flags,
              struct sockaddr *addr, socklen_t *addrlen);
```

Receives data and the sender's address. Used primarily with
`SOCK_DGRAM` (UDP) sockets.

[recvfrom(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=recvfrom&sektion=2) |
[POSIX recvfrom()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/recvfrom.html)

#### sendmsg()

**LVO:** -270 | **Registers:** D0/A0/D1

```c
LONG sendmsg(LONG sock, struct msghdr *msg, LONG flags);
```

Sends a message using a `struct msghdr` for scatter/gather I/O and
ancillary data.

[sendmsg(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=sendmsg&sektion=2) |
[POSIX sendmsg()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/sendmsg.html)

#### recvmsg()

**LVO:** -276 | **Registers:** D0/A0/D1

```c
LONG recvmsg(LONG sock, struct msghdr *msg, LONG flags);
```

Receives a message using a `struct msghdr` for scatter/gather I/O and
ancillary data.

[recvmsg(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=recvmsg&sektion=2) |
[POSIX recvmsg()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/recvmsg.html)

---

### Socket Options and Queries

#### getsockopt()

**LVO:** -96 | **Registers:** D0/D1/D2/A0/A1

```c
LONG getsockopt(LONG sock, LONG level, LONG optname,
                APTR optval, socklen_t *optlen);
```

Retrieves a socket option. Supports `SOL_SOCKET`-level options
(`SO_ERROR`, `SO_KEEPALIVE`, `SO_REUSEADDR`, `SO_RCVBUF`, `SO_SNDBUF`,
`SO_LINGER`, `SO_TYPE`, `SO_EVENTMASK`, etc.) and protocol-level
options (`IPPROTO_TCP` for `TCP_NODELAY`, etc.).

[getsockopt(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getsockopt&sektion=2) |
[POSIX getsockopt()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getsockopt.html)

#### setsockopt()

**LVO:** -90 | **Registers:** D0/D1/D2/A0/D3

```c
LONG setsockopt(LONG sock, LONG level, LONG optname,
                APTR optval, socklen_t optlen);
```

Sets a socket option. The `optval` parameter is typed as `APTR` ---
pass the address of the option value, cast if necessary.

[setsockopt(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=setsockopt&sektion=2) |
[POSIX setsockopt()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/setsockopt.html)

#### getsockname()

**LVO:** -102 | **Registers:** D0/A0/A1

```c
LONG getsockname(LONG sock, struct sockaddr *name, socklen_t *namelen);
```

Returns the local address bound to a socket.

[getsockname(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getsockname&sektion=2) |
[POSIX getsockname()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getsockname.html)

#### getpeername()

**LVO:** -108 | **Registers:** D0/A0/A1

```c
LONG getpeername(LONG sock, struct sockaddr *name, socklen_t *namelen);
```

Returns the remote address of a connected socket.

[getpeername(2) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getpeername&sektion=2) |
[POSIX getpeername()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getpeername.html)

---

### Name Resolution

#### gethostbyname()

**LVO:** -210 | **Registers:** A0

```c
struct hostent *gethostbyname(STRPTR name);
```

Resolves a hostname to an address. Returns a pointer to a static
`struct hostent` or NULL on failure (check h_errno). The returned
structure is per-opener and is overwritten by subsequent calls.

The `name` parameter is typed as `STRPTR`. Cast string literals:
`gethostbyname((STRPTR)"example.com")`.

[gethostbyname(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=gethostbyname&sektion=3)

#### gethostbyaddr()

**LVO:** -216 | **Registers:** A0/D0/D1

```c
struct hostent *gethostbyaddr(STRPTR addr, LONG len, LONG type);
```

Resolves an address to a hostname (reverse DNS). The `addr` parameter
is a pointer to the binary address (e.g., `&sin_addr`), typed as
`STRPTR` in the library prototype --- cast appropriately. `len` is the
address length (4 for IPv4), `type` is the address family (`AF_INET`).

[gethostbyaddr(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=gethostbyaddr&sektion=3)

#### getservbyname()

**LVO:** -234 | **Registers:** A0/A1

```c
struct servent *getservbyname(STRPTR name, STRPTR proto);
```

Looks up a network service by name and protocol (e.g., "http", "tcp").
Returns a pointer to a static `struct servent` or NULL.

[getservbyname(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getservbyname&sektion=3) |
[POSIX getservbyname()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getservbyname.html)

#### getservbyport()

**LVO:** -240 | **Registers:** D0/A0

```c
struct servent *getservbyport(LONG port, STRPTR proto);
```

Looks up a network service by port number (in network byte order) and
protocol. Returns a pointer to a static `struct servent` or NULL.

[getservbyport(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getservbyport&sektion=3) |
[POSIX getservbyport()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getservbyport.html)

#### getprotobyname()

**LVO:** -246 | **Registers:** A0

```c
struct protoent *getprotobyname(STRPTR name);
```

Looks up a protocol by name (e.g., "tcp", "udp"). Returns a pointer to
a static `struct protoent` or NULL.

[getprotobyname(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getprotobyname&sektion=3) |
[POSIX getprotobyname()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getprotobyname.html)

#### getprotobynumber()

**LVO:** -252 | **Registers:** D0

```c
struct protoent *getprotobynumber(LONG proto);
```

Looks up a protocol by its protocol number (e.g., 6 for TCP, 17 for
UDP). Returns a pointer to a static `struct protoent` or NULL.

[getprotobynumber(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getprotobynumber&sektion=3) |
[POSIX getprotobynumber()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/getprotobynumber.html)

#### gethostname()

**LVO:** -282 | **Registers:** A0/D0

```c
LONG gethostname(STRPTR name, LONG namelen);
```

Returns the hostname of the local machine. The `name` parameter is
typed as `STRPTR` (writable buffer), `namelen` is its size. Note the
register assignment: `name` is in A0 and `namelen` is in D0, which
differs from the BSD calling convention.

[gethostname(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=gethostname&sektion=3) |
[POSIX gethostname()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/gethostname.html)

#### gethostid()

**LVO:** -288 | **Registers:** (none)

```c
long gethostid(void);
```

Returns a 32-bit identifier for the local host. On AmigaOS, this
typically returns the primary IP address of the machine.

[gethostid(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=gethostid&sektion=3)

#### getnetbyname()

**LVO:** -222 | **Registers:** A0

```c
struct netent *getnetbyname(STRPTR name);
```

Looks up a network entry by name. Returns a pointer to a static
`struct netent` or NULL. Rarely used in practice.

[getnetbyname(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getnetbyname&sektion=3)

#### getnetbyaddr()

**LVO:** -228 | **Registers:** D0/D1

```c
struct netent *getnetbyaddr(in_addr_t net, LONG type);
```

Looks up a network entry by address and type. Returns a pointer to a
static `struct netent` or NULL. Rarely used in practice.

[getnetbyaddr(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=getnetbyaddr&sektion=3)

---

### Address Conversion (BSD)

#### inet_addr()

**LVO:** -180 | **Registers:** A0

```c
in_addr_t inet_addr(STRPTR cp);
```

Converts a dotted-decimal IPv4 address string to an `in_addr_t` in
network byte order. Returns `INADDR_NONE` (0xFFFFFFFF) on failure.

Note the `INADDR_NONE` ambiguity: the return value for an invalid
string is the same as the valid broadcast address "255.255.255.255".
Use `inet_aton()` (Roadshow extension, LVO -594) to avoid this issue.

The `cp` parameter is typed as `STRPTR`. Cast string literals:
`inet_addr((STRPTR)"127.0.0.1")`.

The bsdsocktest suite tests three cases (tests 108--110):
parsing "127.0.0.1", rejecting "not.an.ip" with `INADDR_NONE`, and
the ambiguity with "255.255.255.255".

[inet_addr(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=inet_addr&sektion=3) |
[POSIX inet_addr()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/inet_addr.html)

#### inet_network()

**LVO:** -204 | **Registers:** A0

```c
in_addr_t inet_network(STRPTR cp);
```

Converts a dotted-decimal network address string to a network number
in **host byte order**. This is the key difference from `inet_addr()`,
which returns in network byte order. On the 68k Amiga (big-endian),
the byte order is the same, but the semantic distinction matters for
portable code.

The bsdsocktest suite verifies that `inet_network("10.0.0.0")` returns
`0x0a000000` in host byte order (test 114).

[inet_network(3) --- FreeBSD](https://man.freebsd.org/cgi/man.cgi?query=inet_network&sektion=3)


## Struct Reference

### struct timeval

```c
#include <devices/timer.h>

struct timeval {
    union { ULONG tv_secs; ULONG tv_sec; };
    union { ULONG tv_micro; ULONG tv_usec; };
};
```

The AmigaOS `struct timeval` (from `<devices/timer.h>`) uses field
names `tv_secs` and `tv_micro`. The SDK provides anonymous unions so
that both the Amiga-native names and the BSD-standard names (`tv_sec`,
`tv_usec`) access the same storage. Both forms are valid.

Used by `WaitSelect()` for the timeout parameter.

### struct sockaddr_in

```c
#include <netinet/in.h>

struct sockaddr_in {
    UBYTE       sin_len;       /* total length (BSD 4.4) */
    sa_family_t sin_family;    /* address family (AF_INET) */
    in_port_t   sin_port;      /* port number (network byte order) */
    struct in_addr sin_addr;   /* IPv4 address */
    UBYTE       sin_zero[8];   /* padding */
};
```

Uses the BSD 4.4 layout with a `sin_len` field. The `sin_len` field
is typically zeroed via `memset()` and ignored by `bind()`/`connect()`
since the address length is passed as a separate parameter. Total size:
16 bytes.

### fd_set

```c
#include <sys/socket.h>

typedef struct fd_set {
    fd_mask fds_bits[howmany(FD_SETSIZE, NFDBITS)];
} fd_set;
```

`FD_SETSIZE` defaults to 256 in the AmiTCP SDK. Manipulated via:
`FD_ZERO(&set)`, `FD_SET(fd, &set)`, `FD_CLR(fd, &set)`,
`FD_ISSET(fd, &set)`. Descriptors at or above `FD_SETSIZE` cannot
be represented.


## Required Headers

| Header                         | Provides                                            |
|--------------------------------|-----------------------------------------------------|
| `<proto/exec.h>`              | `OpenLibrary()`, `CloseLibrary()`, `AllocSignal()`, `FreeSignal()` |
| `<proto/bsdsocket.h>`         | All bsdsocket.library function prototypes (inline stubs). Declares `extern struct Library *SocketBase`. |
| `<sys/socket.h>`              | `AF_*`, `SOCK_*`, `SOL_SOCKET`, `SO_*`, `MSG_*`, `struct sockaddr`, `struct msghdr`, `fd_set`, `FD_*` macros |
| `<netinet/in.h>`              | `struct sockaddr_in`, `struct in_addr`, `in_addr_t`, `IPPROTO_*`, `INADDR_*`, `htonl()`/`ntohl()` macros |
| `<netinet/tcp.h>`             | `TCP_NODELAY` and other TCP-level socket options |
| `<netdb.h>`                   | `struct hostent`, `struct servent`, `struct protoent`, `struct netent` |
| `<libraries/bsdsocket.h>`     | `FD_*` event constants, `UNIQUE_ID`, `SBTF_*`/`SBTB_*`/`SBTC_*` macros, `struct DaemonMessage` |
| `<amitcp/socketbasetags.h>`   | `SBTC_*` tag codes, `SBTM_*` encoding macros (subset also in `<libraries/bsdsocket.h>`) |
| `<sys/syslog.h>`              | `LOG_*` priority and facility constants, `LOG_MASK()`, `LOG_UPTO()`, option flags |
| `<sys/filio.h>`               | `FIONBIO`, `FIONREAD`, `FIOASYNC` ioctl request codes |
| `<devices/timer.h>`           | `struct timeval` (with `tv_secs`/`tv_micro` and `tv_sec`/`tv_usec` union aliases) |


## Appendix: Sources

This reference was compiled from the following primary and secondary
sources.

### Primary Sources (Authoritative)

- **AmiTCP SDK headers**:
  - `clib/bsdsocket_protos.h` --- Function prototypes and parameter
    types
  - `inline/bsdsocket.h` --- Inline stubs with LVO offsets and
    register assignments
  - `libraries/bsdsocket.h` --- `SBTC_*`/`SBTM_*` macros, `UNIQUE_ID`,
    `FD_*` event constants, data structures
  - `amitcp/socketbasetags.h` --- Original AmiTCP tag code definitions
  - `sys/socket.h` --- Socket types, address families, `sockaddr`,
    `fd_set`, `FD_SETSIZE`
  - `netinet/in.h` --- `sockaddr_in`, `in_addr_t`, `IPPROTO_*`,
    classful address macros
  - `sys/syslog.h` --- Syslog priorities, facilities, option flags

- **bsdsocktest source code**:
  - `src/test_transfer.c` --- `Dup2Socket()`, `ObtainSocket()`,
    `ReleaseSocket()`, `ReleaseCopyOfSocket()` behavioral verification
  - `src/test_utility.c` --- `Inet_NtoA()`, `Inet_LnaOf()`,
    `Inet_NetOf()`, `Inet_MakeAddr()`, `inet_addr()`,
    `inet_network()` behavioral verification
  - `src/test_misc.c` --- `vsyslog()` canary test
  - `src/test_socket.c` --- `socket()`, `CloseSocket()` behavioral
    verification
  - `src/test_sendrecv.c` --- `send()`, `recv()`, `sendto()`,
    `recvfrom()` behavioral verification
  - `src/test_sockopt.c` --- `getsockopt()`, `setsockopt()` behavioral
    verification
  - `src/test_waitselect.c` --- `WaitSelect()` behavioral verification
  - `src/test_signals.c` --- `SetSocketSignals()`, signal mask
    behavioral verification
  - `src/test_dns.c` --- DNS resolution function behavioral
    verification
  - `src/test_errno.c` --- `Errno()`, `SetErrnoPtr()`,
    `SocketBaseTagList()` errno handling verification

### Secondary Sources (Reference)

- [BSD Socket Autodoc](https://wiki.amigaos.net/amiga/autodocs/bsdsocket.doc.txt)
  --- AmigaOS wiki mirror of the bsdsocket.library autodoc
- [AmiTCP SDK 4.3](https://aminet.net/package/comm/tcp/AmiTCP-SDK-4.3)
  --- Original AmiTCP/IP SDK distribution on Aminet
- [AROS BSDsocket documentation](https://en.wikibooks.org/wiki/Aros/Developer/Docs/Libraries/BSDsocket)
  --- AROS re-implementation documentation
- [FreeBSD man pages](https://man.freebsd.org/) --- Authoritative BSD
  socket API reference
- [POSIX.1-2017](https://pubs.opengroup.org/onlinepubs/9699919799/) ---
  IEEE Std 1003.1 socket function specifications
