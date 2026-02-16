/*
 * bsdsocktest — ICMP ping tests
 *
 * Tests: raw socket creation, ICMP echo request/reply, RTT measurement,
 *        large payload, multi-ping, timeout on non-routable address.
 *
 * 5 tests (132-136).
 */

#include "tap.h"
#include "testutil.h"
#include "helper_proto.h"

#include <proto/bsdsocket.h>

#include <netinet/in.h>
#include <string.h>

#ifndef IPPROTO_ICMP
#define IPPROTO_ICMP 1
#endif

/* ICMP echo header */
struct icmp_echo {
    UBYTE type;
    UBYTE code;
    UWORD checksum;
    UWORD id;
    UWORD seq;
    /* Payload follows */
};

#define ICMP_ECHO_REQUEST 8
#define ICMP_ECHO_REPLY   0
#define ICMP_ID           0xBD51  /* "BDSocktest 1" */

/* Static buffers (BSS, not stack) */
static UBYTE icmp_sbuf[1500];
static UBYTE icmp_rbuf[1500];

static UWORD icmp_checksum(const UBYTE *data, int len)
{
    ULONG sum = 0;

    while (len > 1) {
        sum += *(const UWORD *)data;
        data += 2;
        len -= 2;
    }
    if (len)
        sum += (UWORD)(*data) << 8;  /* big-endian: trailing byte is MSB */
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return (UWORD)~sum;
}

/* Send ICMP echo request and wait for matching reply.
 * Returns RTT in microseconds, 0 on timeout, -1 on error.
 * LONG range limits RTT to ~35 minutes; safe given the 3-second timeout. */
static LONG icmp_ping(ULONG target_ip, int payload_len, UWORD seq)
{
    LONG rawfd;
    struct icmp_echo *hdr;
    struct sockaddr_in dst;
    struct bst_timestamp ts_start, ts_now;
    LONG timeout_ms;
    ULONG rtt_us;
    int pktlen, ip_hlen;
    struct icmp_echo *reply;
    fd_set readfds;
    struct timeval tv;
    LONG rc, n;

    rawfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (rawfd < 0)
        return -1;

    /* Build ICMP echo request */
    pktlen = 8 + payload_len;
    memset(icmp_sbuf, 0, pktlen);
    hdr = (struct icmp_echo *)icmp_sbuf;
    hdr->type = ICMP_ECHO_REQUEST;
    hdr->code = 0;
    hdr->id = htons(ICMP_ID);
    hdr->seq = htons(seq);
    fill_test_pattern(icmp_sbuf + 8, payload_len, seq);
    hdr->checksum = 0;
    hdr->checksum = icmp_checksum(icmp_sbuf, pktlen);

    /* Send */
    memset(&dst, 0, sizeof(dst));
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = target_ip;

    rc = sendto(rawfd, icmp_sbuf, pktlen, 0,
                (struct sockaddr *)&dst, sizeof(dst));
    if (rc < 0) {
        safe_close(rawfd);
        return -1;
    }

    /* Receive loop with shrinking timeout */
    timer_now(&ts_start);
    timeout_ms = 3000;

    while (timeout_ms > 0) {
        tv.tv_secs = timeout_ms / 1000;
        tv.tv_micro = (timeout_ms % 1000) * 1000;

        FD_ZERO(&readfds);
        FD_SET(rawfd, &readfds);
        rc = WaitSelect(rawfd + 1, &readfds, NULL, NULL, &tv, NULL);
        if (rc <= 0)
            break;

        n = recv(rawfd, icmp_rbuf, sizeof(icmp_rbuf), 0);
        if (n <= 0)
            break;

        /* Parse IP header to find ICMP data */
        ip_hlen = (icmp_rbuf[0] & 0x0F) * 4;
        if (n < ip_hlen + 8)
            goto shrink;

        reply = (struct icmp_echo *)(icmp_rbuf + ip_hlen);
        if (reply->type == ICMP_ECHO_REPLY &&
            reply->id == htons(ICMP_ID) &&
            reply->seq == htons(seq)) {
            /* Matching reply — compute RTT */
            timer_now(&ts_now);
            rtt_us = timer_elapsed_us(&ts_start, &ts_now);

            /* Verify payload integrity */
            if (n >= ip_hlen + 8 + payload_len) {
                int mm = verify_test_pattern(
                    icmp_rbuf + ip_hlen + 8, payload_len, seq);
                if (mm)
                    tap_diagf("  ICMP payload mismatch at offset %d", mm);
            }

            safe_close(rawfd);
            return (LONG)(rtt_us > 0 ? rtt_us : 1);
        }

shrink:
        /* Non-matching packet — shrink remaining timeout */
        timer_now(&ts_now);
        timeout_ms = 3000 - (LONG)timer_elapsed_ms(&ts_start, &ts_now);
    }

    safe_close(rawfd);
    return 0;  /* Timeout */
}

void run_icmp_tests(void)
{
    LONG rawfd;
    LONG rtt;
    LONG rtts[5];
    int success, i;
    LONG rtt_min, rtt_max, rtt_sum;

    /* Check if raw ICMP sockets are available */
    rawfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (rawfd < 0) {
        tap_skip("SOCK_RAW/ICMP not supported");
        CHECK_CTRLC();
        tap_skip("SOCK_RAW/ICMP not supported");
        CHECK_CTRLC();
        tap_skip("SOCK_RAW/ICMP not supported");
        CHECK_CTRLC();
        tap_skip("SOCK_RAW/ICMP not supported");
        CHECK_CTRLC();
        tap_skip("SOCK_RAW/ICMP not supported");
        return;
    }
    safe_close(rawfd);

    /* 132. icmp_loopback */
    rtt = icmp_ping(htonl(INADDR_LOOPBACK), 56, 1);
    tap_ok(rtt > 0, "ICMP echo: loopback 127.0.0.1 [RFC 792]");
    if (rtt > 0) {
        tap_diagf("  RTT=%ld.%03ldms", (long)(rtt / 1000), (long)(rtt % 1000));
        tap_notef("Loopback RTT: %ld.%03ldms",
                  (long)(rtt / 1000), (long)(rtt % 1000));
    } else {
        tap_diagf("  result=%ld", (long)rtt);
    }

    CHECK_CTRLC();

    /* Network ICMP tests — gated by helper */
    if (!helper_is_connected()) {
        tap_skip("host helper not connected");
        CHECK_CTRLC();
        tap_skip("host helper not connected");
        CHECK_CTRLC();
        tap_skip("host helper not connected");
        CHECK_CTRLC();
    } else {
        /* 133. icmp_network */
        rtt = icmp_ping(helper_addr(), 56, 2);
        tap_ok(rtt > 0, "ICMP echo: network host [RFC 792]");
        if (rtt > 0) {
            tap_diagf("  RTT=%ld.%03ldms, target=%s",
                      (long)(rtt / 1000), (long)(rtt % 1000),
                      Inet_NtoA(helper_addr()));
            tap_notef("Network RTT: %ld.%03ldms",
                      (long)(rtt / 1000), (long)(rtt % 1000));
        } else {
            tap_diagf("  result=%ld", (long)rtt);
        }

        CHECK_CTRLC();

        /* 134. icmp_large_payload */
        rtt = icmp_ping(helper_addr(), 1024, 3);
        tap_ok(rtt > 0, "ICMP echo: 1024-byte payload [RFC 792]");
        if (rtt > 0)
            tap_diagf("  RTT=%ld.%03ldms, payload=1024",
                      (long)(rtt / 1000), (long)(rtt % 1000));
        else
            tap_diagf("  result=%ld", (long)rtt);

        CHECK_CTRLC();

        /* 135. icmp_multi_ping */
        success = 0;
        rtt_min = 0x7FFFFFFF;
        rtt_max = 0;
        rtt_sum = 0;
        for (i = 0; i < 5; i++) {
            rtts[i] = icmp_ping(helper_addr(), 56, (UWORD)(10 + i));
            if (rtts[i] > 0) {
                success++;
                if (rtts[i] < rtt_min) rtt_min = rtts[i];
                if (rtts[i] > rtt_max) rtt_max = rtts[i];
                rtt_sum += rtts[i];
            }
        }
        tap_ok(success >= 4, "ICMP echo: multiple pings reliability [RFC 792]");
        tap_diagf("  received=%d/5", success);
        if (success > 0) {
            LONG rtt_avg = rtt_sum / success;
            tap_diagf("  RTT min=%ld.%03ldms max=%ld.%03ldms avg=%ld.%03ldms",
                      (long)(rtt_min / 1000), (long)(rtt_min % 1000),
                      (long)(rtt_max / 1000), (long)(rtt_max % 1000),
                      (long)(rtt_avg / 1000), (long)(rtt_avg % 1000));
        }
        tap_notef("Multi-ping: %d/5 replies", success);
    }

    CHECK_CTRLC();

    /* 136. icmp_timeout */
    rtt = icmp_ping(inet_addr((STRPTR)"192.0.2.1"), 56, 99);
    if (rtt == 0) {
        tap_ok(1, "ICMP echo: timeout on unreachable host [RFC 792]");
        tap_diag("  192.0.2.1 (TEST-NET-1): no reply within 3s");
    } else if (rtt < 0) {
        tap_ok(1, "ICMP echo: timeout on unreachable host [RFC 792]");
        tap_diagf("  errno=%ld (e.g. ENETUNREACH without default route)",
                  (long)get_bsd_errno());
    } else {
        tap_ok(0, "ICMP echo: timeout on unreachable host [RFC 792]");
        tap_diagf("  unexpected reply, RTT=%ld.%03ldms",
                  (long)(rtt / 1000), (long)(rtt % 1000));
    }
}
