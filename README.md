# bsdsocktest

An open-source conformance test suite for Amiga **bsdsocket.library** --- the
BSD socket API implemented by all Amiga TCP/IP stacks (Roadshow, AmiTCP,
Miami, Genesis) and emulators (Amiberry, WinUAE). The suite exercises 142
tests across 12 categories covering socket lifecycle, data transfer, async
I/O, name resolution, descriptor transfer, throughput benchmarks, and more.
Cross-compiled C targeting m68k AmigaOS (68020+).

## Documentation

- [docs/TESTS.md](docs/TESTS.md) --- Per-test reference covering all 142 tests: what each validates, methodology, and expected behavior
- [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) --- Known issues per TCP/IP stack, with root cause analysis and fix priorities
- [docs/AMITCP_API.md](docs/AMITCP_API.md) --- Programmer's reference for the Amiga bsdsocket.library API, focusing on differences from standard BSD sockets
- [host/README.md](host/README.md) --- Setup and usage guide for the host helper script required by network-tier tests

## Quick Start

**Requirements:** AmigaOS 2.0+, 68020+, and a TCP/IP stack providing
bsdsocket.library (Roadshow, AmiTCP, Miami, Genesis, or emulator equivalent).

Download `bsdsocktest.lha` from the
[Releases](https://github.com/tbdye/bsdsocktest/releases) page and extract it
to your Amiga.

**From the CLI:**

```
bsdsocktest
```

This runs all loopback tests (no network required) and writes results to both
the screen and `bsdsocktest.log` in the current directory.

**From Workbench:**

Double-click the `bsdsocktest` icon. A console window opens automatically.
Configure options via the icon's Tool Types (HOST, VERBOSE, LOOPBACK, etc.).

## Building from Source

### Prerequisites

- [m68k-amigaos-gcc](https://github.com/AmigaPorts/m68k-amigaos-gcc) cross-compiler
  toolchain, installed at `/opt/amiga` (or override with `PREFIX`)

### Build

```
make
```

The binary is written to `bsdsocktest` in the project root. To specify a
different toolchain location:

```
make PREFIX=/path/to/amiga
```

The compiler flags are `-noixemul -O2 -Wall -Wextra -m68020 -fomit-frame-pointer`.

### Clean

```
make clean
```

## Usage

The ReadArgs template:

```
CATEGORY/K,HOST/K,PORT/N,LOG/K,ALL/S,LOOPBACK/S,NETWORK/S,LIST/S,VERBOSE/S,NOPAGE/S
```

| Parameter  | Description |
|------------|-------------|
| `CATEGORY` | Run a single test category by name (e.g., `CATEGORY socket`) |
| `HOST`     | IP address of the host helper for network tests |
| `PORT`     | Base port number for test sockets (default: 7700) |
| `LOG`      | Log file path (default: `bsdsocktest.log`; use `NIL:` to suppress) |
| `ALL`      | Run all test categories (this is the default) |
| `LOOPBACK` | Run only loopback (self-contained) tests |
| `NETWORK`  | Run only network tests (requires host helper) |
| `LIST`     | List available test categories and exit |
| `VERBOSE`  | Show individual test results on screen |
| `NOPAGE`   | Disable pagination (output scrolls freely) |

### Examples

```
bsdsocktest                            ; Run all tests (loopback only without HOST)
bsdsocktest HOST 192.168.1.10          ; Run all tests including network categories
bsdsocktest CATEGORY dns HOST 10.0.0.1 ; Run only DNS tests with host helper
bsdsocktest LOOPBACK VERBOSE           ; Loopback tests with per-test detail
bsdsocktest LIST                       ; Show available categories
```

## Test Categories

| Category      | Tests | Tier      | Description |
|---------------|------:|-----------|-------------|
| `socket`      |    23 | loopback  | Core socket lifecycle: create, bind, listen, connect, accept, close |
| `sendrecv`    |    19 | both      | Data transfer: send, recv, sendto, recvfrom, sendmsg, recvmsg |
| `sockopt`     |    15 | loopback  | Socket options: getsockopt, setsockopt, IoctlSocket |
| `waitselect`  |    15 | loopback  | Async I/O: WaitSelect readiness, timeout, signal integration |
| `signals`     |    15 | loopback  | Signals and events: SetSocketSignals, SocketBaseTags, GetSocketEvents |
| `dns`         |    17 | both      | Name resolution: gethostbyname/addr, getservby\*, getprotoby\* |
| `utility`     |    10 | loopback  | Address utilities: Inet\_NtoA, inet\_addr, Inet\_LnaOf, Inet\_NetOf |
| `transfer`    |     5 | loopback  | Descriptor transfer: Dup2Socket, ObtainSocket, ReleaseSocket |
| `errno`       |     7 | loopback  | Error handling: Errno, SetErrnoPtr, SocketBaseTags errno pointers |
| `misc`        |     5 | loopback  | Miscellaneous: getdtablesize, syslog, resource limits |
| `icmp`        |     5 | both      | ICMP echo: raw socket ping, RTT measurement |
| `throughput`  |     6 | both      | Throughput benchmarks: TCP/UDP loopback and network transfer |
| **Total**     | **142** | | |

**Tier legend:** "loopback" tests are self-contained (no network needed).
"both" categories contain a mix of loopback and network tests; network tests
within a "both" category are skipped when the host helper is not connected.

## Understanding Results

### Screen output

In default (compact) mode, each category shows a one-line summary:

```
socket................. 23/23 passed
sendrecv............... 16/16 passed (3 skipped)
```

When unexpected failures occur, they are expanded below the category line.
Known stack limitations are counted separately and do not cause a failure
result.

The final summary line shows the aggregate:

```
Results: 138/138 passed (4 known issues, 4 skipped)
```

### Verbose mode

With `VERBOSE`, individual test results appear on screen in addition to the
category summaries:

```
    1 ok    - socket(SOCK_STREAM) returns valid fd
    2 ok    - socket(SOCK_DGRAM) returns valid fd
    3 KNOWN - recv(MSG_OOB) returns EINVAL
```

### Log file

A TAP (Test Anything Protocol) version 12 log is always written (default:
`bsdsocktest.log`). The log contains the full machine-readable output
including plan lines, individual test results, diagnostics, and known-failure
annotations. Use `LOG NIL:` to suppress log file creation.

### Known failures

The suite includes a data-driven known-failures system. When a detected
TCP/IP stack has documented behavioral deviations, matching test failures are
annotated as `KNOWN <stack>: <reason>` rather than counted as unexpected
failures. This allows clean pass/fail reporting without masking real issues.

### Exit codes

| Code | AmigaOS Constant | Meaning |
|-----:|------------------|---------|
|    0 | `RETURN_OK`      | All tests passed (known failures are acceptable) |
|    5 | `RETURN_WARN`    | One or more unexpected failures |
|   20 | `RETURN_FAIL`    | Bail out (library unavailable, Ctrl-C, etc.) |

## Host Helper

Network-tier tests (sendrecv, dns, icmp, throughput) require a host helper
running on a machine reachable from the Amiga over the network.

### Quick start

On the host machine (Python 3.6+, no extra dependencies):

```
python3 host/bsdsocktest_helper.py
```

Then on the Amiga:

```
bsdsocktest HOST <host-ip>
```

Without the HOST argument, network tests are automatically skipped (12 tests).
If HOST is specified but the helper is not running, the test suite will bail
out. See [host/README.md](host/README.md) for detailed host helper
documentation.

## Tested Stacks

### Roadshow 1.15 (bsdsocket.library 4.364)

With the host helper connected, all 142 tests are executed:

- **138 passed**
- **4 known issues** (documented stack deviations)
- **0 unexpected failures**

| Test | Category | Description |
|-----:|----------|-------------|
|   27 | sendrecv | `recv(MSG_OOB)` returns `EINVAL` --- OOB data not supported |
|   35 | sendrecv | Loopback does not generate RST for closed peer |
|   76 | signals  | `SBTC_ERRNOLONGPTR` GET not supported (SET-only) |
|   77 | signals  | `SBTC_HERRNOLONGPTR` GET not supported (SET-only) |

## Standards References

Tests are tagged with standards citations to document the expected behavior:

- **[BSD 4.4]** --- 4.4BSD socket semantics (the baseline)
- **[POSIX.1]** --- IEEE Std 1003.1 / Single UNIX Specification
- **[RFC 793]**, **[RFC 768]**, etc. --- Protocol-level requirements
- **[AmiTCP]** --- AmiTCP SDK documentation (the original bsdsocket.library spec)
- **[Roadshow]** --- Roadshow-specific extensions or behaviors

## License

This project is licensed under the
[GNU General Public License v3.0](LICENSE).
