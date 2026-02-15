# bsdsocktest Host Helper

## Overview

The host helper (`bsdsocktest_helper.py`) is a Python server that runs on a
machine reachable from the Amiga over the network. It provides the services
needed by bsdsocktest's network-tier tests: TCP and UDP echo, data sink, data
source, and active connection initiation (connect-to-Amiga).

Without the host helper, network tests are automatically skipped. Loopback
tests run without it.

## Requirements

- Python 3.6 or later
- No third-party dependencies (standard library only)
- Network connectivity between the host and the Amiga

## Usage

```
python3 bsdsocktest_helper.py [-v] [--bind ADDR] [--ctrl-port PORT]
```

| Option         | Default     | Description |
|----------------|-------------|-------------|
| `--bind ADDR`  | `0.0.0.0`  | Bind address for all listeners |
| `--ctrl-port PORT` | `8700` | Control channel port (other services use consecutive ports) |
| `-v, --verbose` | off        | Enable verbose logging (per-connection detail) |

### Starting

```
python3 bsdsocktest_helper.py
```

The helper prints its listening ports to stderr and runs until interrupted:

```
[helper] Listening on 0.0.0.0
[helper]   Control:    port 8700
[helper]   TCP echo:   port 8701
[helper]   UDP echo:   port 8702
[helper]   TCP sink:   port 8703
[helper]   TCP source: port 8704
```

### Stopping

Press Ctrl-C. The helper closes all connections and exits cleanly.

### Custom port range

If the default ports conflict with other services, the helper can listen on
a different base port:

```
python3 bsdsocktest_helper.py --ctrl-port 9700
```

Note: The Amiga binary has the helper control port (8700) compiled in.
Changing `--ctrl-port` on the helper requires recompiling the Amiga binary
with a matching `HELPER_CTRL_PORT` value in `helper_proto.h`. The `PORT`
parameter on the Amiga controls the test socket base port (default 7700),
not the helper port.

## Services

| Port | Protocol | Service    | Description |
|-----:|----------|------------|-------------|
| 8700 | TCP      | Control    | Line-based command channel for test coordination |
| 8701 | TCP      | TCP echo   | Echoes all received data back to the sender |
| 8702 | UDP      | UDP echo   | Echoes each datagram back to the sender |
| 8703 | TCP      | TCP sink   | Receives and discards all data (for send throughput tests) |
| 8704 | TCP      | TCP source | Sends a repeating test pattern until the client disconnects |

Port numbers shown assume the default `--ctrl-port 8700`. All service ports
are at fixed offsets from the control port: ctrl+1 through ctrl+4.

## Control Protocol

The control channel uses a simple line-based text protocol (newline-terminated
ASCII). The Amiga connects to the control port at test startup and
disconnects at the end.

### Handshake

1. Amiga connects to the control port
2. Helper sends `OK\n`
3. Helper records the Amiga's IP address from the connection

### Commands

| Command          | Response | Description |
|------------------|----------|-------------|
| `CONNECT <port>` | `GO\n`   | Helper connects to the Amiga on the specified port (used by accept tests) |
| `QUIT`           | (none)   | Helper closes the control connection |

**CONNECT flow:**

1. Amiga sends `CONNECT <port>\n`
2. Helper responds `GO\n` immediately
3. Helper waits 500ms (for the Amiga to set up its listener), then connects
   to `<amiga-ip>:<port>`, sends `BSDSOCKTEST HELLO FROM HELPER\n`, and
   closes the connection

If the Amiga's IP is not known or the port is invalid, the helper responds
with `FAIL <reason>\n`.

## Troubleshooting

**"Could not connect to host helper" on the Amiga:**
- Verify the helper is running and the IP address is correct
- Check that port 8700 (TCP) is reachable from the Amiga (firewall rules)
- Try `--bind 0.0.0.0` if the host has multiple interfaces

**Network tests skip even with HOST set:**
- The helper must be running before `bsdsocktest` starts
- If the control connection fails, all network tests are skipped (not failed)

**CONNECT command fails:**
- The Amiga must be listening on the requested port before the helper
  attempts to connect (the 500ms delay should be sufficient)
- Verify bidirectional connectivity: the helper must be able to connect
  *to* the Amiga, not just the other way around

**Port conflicts:**
- If port 8700 is already in use, start the helper with `--ctrl-port <N>`
- The Amiga's `PORT` parameter controls test socket ports (7700+), not the
  helper control port

**Verbose mode:**
- Use `-v` to see per-connection and per-command logging on stderr, which
  helps diagnose protocol-level issues
