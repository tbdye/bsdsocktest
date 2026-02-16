# bsdsocktest --- Stack Compatibility

Known issues discovered by bsdsocktest, organized by TCP/IP stack. Each
entry lists the test number, the observed behavior, and what a conforming
implementation should do instead. Test numbers link to the corresponding
entry in [TESTS.md](TESTS.md).

These issues are tracked in the test suite's `known_failures.c` data tables
and are matched by exact version string. When a stack ships a fix and
changes its version string, the entries stop applying automatically.

At runtime, known failures are annotated in the TAP log as
`# KNOWN <stack>: <reason>` and counted separately from unexpected failures.
Known crashes are skipped entirely to avoid crashing the emulator.

---

## Roadshow (tested against 4.364)

Version string: `Roadshow 4.364` via `SBTC_RELEASESTRPTR`.

Roadshow is a mature, commercially developed TCP/IP stack. These 4 issues
are minor edge cases that do not affect normal application behavior.

### Failures (4)

| Test | Description | Issue |
|-----:|-------------|-------|
| 27 | recv(MSG_OOB): urgent data delivery | Returns `EINVAL`. OOB data is not implemented on Roadshow's loopback path. |
| 35 | send(): error after peer closes connection | Loopback does not generate RST when the peer closes. Multiple `send()` attempts never produce `EPIPE`/`ECONNRESET`. |
| 76 | SocketBaseTags(SBTC_ERRNOLONGPTR): get errno pointer | GET via `SBTM_GETREF` returns NULL. Roadshow supports SET but not readback of the registered errno pointer. |
| 77 | SocketBaseTags(SBTC_HERRNOLONGPTR): get h_errno pointer | GET via `SBTM_GETREF` returns NULL. Same as test 76 but for the h_errno pointer. |

---

## Amiberry 7.1.1 bsdsocket emulation (tested against UAE 7.1.1)

Version string: `UAE 7.1.1` via `SBTC_RELEASESTRPTR`. This is the current
stable release (Amiberry 7.1.1 Debian package). The version tracks the
Amiberry release version (`project(amiberry VERSION X.Y.Z)` in
CMakeLists.txt).

The bsdsocket emulation intercepts Amiga socket library calls and maps them
to host-side socket operations. It does not use SANA-II or any NIC-level
emulation. Enable with `bsdsocket_emu=true` in the .uae configuration.

Note that fixes for all issues listed below have been submitted and merged
upstream. They will ship in the next Amiberry release.

### Crashes (9)

These operations cause the emulator to `exit(1)`. The test suite skips them
to avoid killing the emulator process.

| Test | Description | Root Cause |
|-----:|-------------|------------|
| 70 | WaitSelect(): >64 descriptors | Internal fd mapping array hardcoded to 64 entries. `nfds > 64` causes out-of-bounds access. Also triggered by `SBTC_DTABLESIZE SET` (test 78 guards against this). |
| 79 | SO_EVENTMASK FD_READ: signal on data arrival | Setting `SO_EVENTMASK` via `setsockopt()` starts an event monitor thread that immediately causes `exit(1)`. |
| 80 | SO_EVENTMASK FD_CONNECT: signal on connect | Same as test 79. |
| 81 | SO_EVENTMASK: no spurious events on idle socket | Same as test 79. |
| 82 | SO_EVENTMASK FD_ACCEPT: signal on incoming | Same as test 79. |
| 83 | SO_EVENTMASK FD_CLOSE: signal on peer disconnect | Same as test 79. |
| 84 | GetSocketEvents(): event consumed after retrieval | Same as test 79. |
| 85 | GetSocketEvents(): round-robin across sockets | Same as test 79. |
| 87 | WaitSelect + signals: stress test (50 iterations) | Same as test 79. |

### Failures (22)

These tests run but produce incorrect results.

#### Stale errno (8 tests)

The emulation's worker thread does not clear `sb_errno` on success, so a
stale error value from a prior operation persists and contaminates
subsequent calls. This is the root cause of 8 failures, 6 of which are
intermittent collateral damage.

Deterministic:

| Test | Description | Detail |
|-----:|-------------|--------|
| 125 | SBTC_ERRNOLONGPTR test | Stale EBADF not replaced by ECONNREFUSED after connect to closed port. `second=9 (expected 61)`. |
| 126 | connect(): not affected by stale errno | `connect()` returns -1/EBADF when errno contains a stale value from a prior failed call. The worker thread reads `sb_errno` as input to determine the return value. |

Intermittent (depend on prior test execution order):

| Test | Description | Detail |
|-----:|-------------|--------|
| 12 | connect() to closed port | Reports stale errno instead of ECONNREFUSED. |
| 15 | accept() on non-blocking socket | Reports stale errno instead of EWOULDBLOCK. |
| 33 | recv() on empty non-blocking socket | Reports stale errno instead of EWOULDBLOCK. |
| 35 | send() after peer close | Stale errno prevents EPIPE/ECONNRESET detection. `1 attempts without error, last errno: 0`. |
| 52 | SO_ERROR after failed connect | `getsockopt(SO_ERROR)` returns 0 instead of the pending error. Stale errno contaminates the non-blocking connect path. |
| 55 | IoctlSocket(FIONBIO) | Errno shows 0 after expected success (should show EINPROGRESS=36 from the prior non-blocking set). |

#### sendmsg/recvmsg (2 tests)

| Test | Description | Detail |
|-----:|-------------|--------|
| 31 | sendmsg(): single iovec | `host_sendto()` uses `get_real_address(msg)` instead of `realpt` for `sb->buf` -- when called from sendmsg with msg=0, sends from Amiga address 0. |
| 32 | recvmsg(): scatter-gather (multiple iovecs) | `bsdsocklib_recvmsg()` uses `ftable[sd-1]` with 0-based sd -- off-by-one in MSG_TRUNC detection. |

#### Socket options (2 tests)

| Test | Description | Detail |
|-----:|-------------|--------|
| 49 | SO_RCVTIMEO: set receive timeout | `setsockopt()` succeeds but `getsockopt()` fails with EINVAL. Amiga passes 8-byte `struct timeval` (optlen=8) but host kernel on 64-bit expects `sizeof(struct timeval)=16`. |
| 50 | SO_SNDTIMEO: set send timeout | Same as test 49. |

#### WaitSelect / descriptor table (3 tests)

| Test | Description | Detail |
|-----:|-------------|--------|
| 63 | WaitSelect(): all NULL fdsets + timeout = delay | Returns immediately (elapsed 0ms) instead of honoring the timeout as a pure delay. |
| 78 | SocketBaseTags(SBTC_DTABLESIZE): get/set table size | `SBTM_GETREF(SBTC_DTABLESIZE)` returns 0 instead of 64. `getdtablesize()` correctly returns 64 via a separate code path (test 127 passes). SET is not tested because it would crash (guarded by GET check). |
| 128 | getdtablesize(): reflects SBTC_DTABLESIZE change | Cannot be tested because the prerequisite SBTC_DTABLESIZE GET returns 0 (same underlying bug as test 78). |

#### DNS / name resolution (3 tests)

| Test | Description | Detail |
|-----:|-------------|--------|
| 93 | getservbyname(): unknown service returns NULL | Returns stale `sb->servent` pointer instead of NULL. Prior allocation not freed/cleared. |
| 94 | getservbyport(): port 21/"tcp" -> "ftp" | Returns "http" instead of "ftp". Amiga network-byte-order port not converted via `htons()` before host lookup. |
| 98 | gethostname(): retrieve hostname | Returns empty string. Logic reversed: reads FROM Amiga memory instead of writing the hostname TO it. |

#### Address utilities (3 tests)

| Test | Description | Detail |
|-----:|-------------|--------|
| 111 | Inet_LnaOf(): extract host part | Returns 0x000000 instead of 0x010203 for address 10.1.2.3. The function is a stub. |
| 112 | Inet_NetOf(): extract network part | Returns 0x00 instead of 0x0a for address 10.1.2.3. The function is a stub. |
| 113 | Inet_MakeAddr(): round-trip with LnaOf/NetOf | Returns 0x00000000. Broken because `Inet_LnaOf()` and `Inet_NetOf()` both return 0. |

#### Descriptor transfer (1 test)

| Test | Description | Detail |
|-----:|-------------|--------|
| 116 | Dup2Socket(fd, target): duplicate to specific slot | Returns 0 instead of target fd. The `target` parameter is ignored; behaves like `Dup2Socket(fd, -1)`. Also missing return -1 on bounds check failure. |

---

## Amiberry bsdsocket emulation (tested against UAE 8.0.0)

Version string: `UAE 8.0.0` via `SBTC_RELEASESTRPTR`. The version tracks
the Amiberry release version (`project(amiberry VERSION X.Y.Z)` in
CMakeLists.txt).

The bsdsocket emulation intercepts Amiga socket library calls and maps them
to host-side socket operations. It does not use SANA-II or any NIC-level
emulation. Enable with `bsdsocket_emu=true` in the .uae configuration.

Amiberry 8.0.0 (development master) includes fixes for all 31 issues found
in 7.1.1. No known issues remain.

### Results

139 passed, 0 failed, 3 skipped (142 total).

The 3 skipped tests are environmental:

| Test | Reason |
|-----:|--------|
| 34 | Host kernel send buffer exceeds 1 MB; cannot trigger "buffer full" condition. |
| 101 | No `/etc/networks` database; `getnetbyname()` has nothing to look up. |
| 102 | No `/etc/networks` database; `getnetbyaddr()` has nothing to look up. |

---

## WinUAE bsdsocket emulation (tested against UAE 6.0.2)

Version string: `UAE 6.0.2` via `SBTC_RELEASESTRPTR`. The version number
reflects the WinUAE product release.

The bsdsocket emulation intercepts Amiga socket library calls and maps them
to host-side socket operations. It does not use SANA-II or any NIC-level
emulation.

### Crashes (1)

| Context | Description | Detail |
|---------|-------------|--------|
| `CloseLibrary(SocketBase)` | Emulator terminates during library cleanup | Calling `CloseLibrary()` on an open bsdsocket.library base after normal socket operations causes the emulator process to terminate silently. Test results are not affected (all output is flushed before cleanup), but the emulator is lost. Confirmed by skipping the `CloseLibrary()` call, which eliminates the crash. |

### Hangs (8)

These tests set `SO_EVENTMASK` via `setsockopt()` and then wait for an
Amiga signal via `WaitSelect()` with a non-NULL signal mask. The
`setsockopt()` call succeeds (no error, no crash), but the signal is never
delivered when the monitored event occurs. `WaitSelect()` with a non-NULL
signal mask blocks indefinitely instead of honoring the timeout. The test
suite skips these to avoid hanging.

| Test | Description | Detail |
|-----:|-------------|--------|
| 79 | SO_EVENTMASK FD_READ: signal on data arrival | Signal not delivered after `send()` fills receive buffer. |
| 80 | SO_EVENTMASK FD_CONNECT: signal on connect | Signal not delivered after non-blocking `connect()` completes. |
| 81 | SO_EVENTMASK: no spurious events on idle socket | Skipped (depends on SO_EVENTMASK infrastructure). |
| 82 | SO_EVENTMASK FD_ACCEPT: signal on incoming | Signal not delivered after incoming connection. |
| 83 | SO_EVENTMASK FD_CLOSE: signal on peer disconnect | Signal not delivered after peer closes connection. |
| 84 | GetSocketEvents(): event consumed after retrieval | Skipped (depends on SO_EVENTMASK infrastructure). |
| 85 | GetSocketEvents(): round-robin across sockets | Skipped (depends on SO_EVENTMASK infrastructure). |
| 87 | WaitSelect + signals: stress test (50 iterations) | Skipped (uses SO_EVENTMASK). |

### Failures (12)

#### Socket options

| Test | Description | Detail |
|-----:|-------------|--------|
| 48 | SO_LINGER: set and read back linger struct | `getsockopt()` does not return the value set by `setsockopt()`. |
| 52 | SO_ERROR: pending error after failed connect | `getsockopt(SO_ERROR)` returns 0 after a failed `connect()` instead of the pending error code. |

#### Send/recv

| Test | Description | Detail |
|-----:|-------------|--------|
| 35 | send(): error after peer closes connection | Returns errno 53 (`ECONNABORTED`) after 1 attempt. Test expects `EPIPE`/`ECONNRESET`. |

#### WaitSelect / descriptor table

| Test | Description | Detail |
|-----:|-------------|--------|
| 63 | WaitSelect(): all NULL fdsets + timeout = delay | Returns immediately (elapsed 0ms) instead of honoring the timeout as a pure delay. |
| 69 | WaitSelect(): nfds = highest_fd + 1 | `WaitSelect()` with `nfds=fd` (should exclude fd) returns the same result as `nfds=fd+1`. The `nfds` upper bound is not enforced. |
| 78 | SocketBaseTags(SBTC_DTABLESIZE): get/set table size | `SBTM_GETVAL(SBTC_DTABLESIZE)` returns 0 instead of 128. Note: `getdtablesize()` correctly returns 128 (test 127), indicating a separate working code path. |
| 128 | getdtablesize(): reflects SBTC_DTABLESIZE change | Cannot be tested because the prerequisite SBTC_DTABLESIZE GET returns 0. |

#### DNS / name resolution

| Test | Description | Detail |
|-----:|-------------|--------|
| 98 | gethostname(): retrieve hostname | Returns success (rc=0) but the hostname string is empty. |

#### Address utilities

| Test | Description | Detail |
|-----:|-------------|--------|
| 111 | Inet_LnaOf(): extract host part | Returns 0x000000 instead of 0x010203 for address 10.1.2.3. The function appears to be a stub. |
| 112 | Inet_NetOf(): extract network part | Returns 0x00 instead of 0x0a for address 10.1.2.3. The function appears to be a stub. |
| 113 | Inet_MakeAddr(): round-trip with LnaOf/NetOf | Returns 0x00000000 instead of 0x0a010203. Broken because `Inet_LnaOf()` and `Inet_NetOf()` both return 0. |

#### Unimplemented functions

| Test | Description | Detail |
|-----:|-------------|--------|
| 116 | Dup2Socket(fd, target): duplicate to specific slot | `Dup2Socket(fd, 10)` returns fd 0 instead of target slot 10. The `target` parameter is ignored. |
