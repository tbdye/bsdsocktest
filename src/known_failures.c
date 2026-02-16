/*
 * bsdsocktest — Known failure and crash detection per TCP/IP stack
 *
 * Auto-detects the running stack from SBTC_RELEASESTRPTR and looks up
 * known issues in a per-stack table.
 *
 * Two types of entries:
 *   KNOWN_FAILURE — test runs and fails; framework annotates as "known"
 *   KNOWN_CRASH   — test would crash the emulator; must be skipped
 *
 * Matching: the detected version string (e.g. "UAE 8.0.0") is compared
 * against each profile's match_version using exact string match.
 * Unrecognized stacks get no annotations.
 */

#include "known_failures.h"

#include <stddef.h>
#include <string.h>

/* ---- Entry types ---- */

enum known_type {
    KNOWN_FAILURE,  /* test runs, fails — annotated as known */
    KNOWN_CRASH     /* test skipped — would crash emulator */
};

struct known_entry {
    int test_number;
    enum known_type type;
    const char *reason;
};

struct stack_profile {
    const char *match_version;      /* exact match against version string */
    const char *stack_name;         /* display name */
    const struct known_entry *entries;
    int entry_count;
};

/* ---- Roadshow (verified against 4.364) ---- */

static const struct known_entry roadshow_entries[] = {
    { 27, KNOWN_FAILURE, "recv(MSG_OOB) returns EINVAL" },
    { 35, KNOWN_FAILURE, "loopback does not generate RST for closed peer" },
    { 76, KNOWN_FAILURE, "SBTC_ERRNOLONGPTR GET not supported (SET-only)" },
    { 77, KNOWN_FAILURE, "SBTC_HERRNOLONGPTR GET not supported (SET-only)" },
};

/* ---- Amiberry bsdsocket emulation (verified against UAE 8.0.0) ---- */

static const struct known_entry amiberry_entries[] = {
    /* Crashes: exercising these operations causes exit(1) */
    { 70, KNOWN_CRASH,   "WaitSelect >64 fds crashes emulator" },
    /* Failures: tests run but produce wrong results */
    { 31, KNOWN_FAILURE, "sendmsg() not implemented" },
    { 32, KNOWN_FAILURE, "recvmsg() not implemented" },
    { 49, KNOWN_FAILURE, "SO_RCVTIMEO set/get roundtrip fails" },
    { 50, KNOWN_FAILURE, "SO_SNDTIMEO set/get roundtrip fails" },
    { 78, KNOWN_FAILURE, "SBTC_DTABLESIZE GET returns 0" },
    { 81, KNOWN_FAILURE, "SO_EVENTMASK fires spurious event on idle socket" },
    { 83, KNOWN_FAILURE, "SO_EVENTMASK FD_CLOSE not delivered on peer disconnect" },
    { 93, KNOWN_FAILURE, "getservbyname() unknown service not returning NULL" },
    { 94, KNOWN_FAILURE, "getservbyport() returns wrong service name" },
    { 98, KNOWN_FAILURE, "gethostname() returns empty string" },
    { 111, KNOWN_FAILURE, "Inet_LnaOf() returns 0" },
    { 112, KNOWN_FAILURE, "Inet_NetOf() returns 0" },
    { 113, KNOWN_FAILURE, "Inet_MakeAddr() returns 0 (LnaOf/NetOf broken)" },
    { 116, KNOWN_FAILURE, "Dup2Socket() to specific slot not implemented" },
    { 126, KNOWN_FAILURE, "stale errno causes connect() EBADF" },
    { 128, KNOWN_FAILURE, "SBTC_DTABLESIZE GET returns 0" },
};

/* ---- WinUAE bsdsocket emulation (verified against UAE 6.0.2) ---- */

static const struct known_entry winuae_entries[] = {
    /* Hangs: SO_EVENTMASK sets up but signal never fires; WaitSelect
       blocks forever instead of honoring timeout when sigmask is set */
    { 79, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    { 80, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    { 81, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    { 82, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    { 83, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    { 84, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    { 85, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    { 87, KNOWN_CRASH,   "SO_EVENTMASK hangs (signal never delivered)" },
    /* Failures: tests run but produce wrong results */
    { 35, KNOWN_FAILURE, "send after peer close returns wrong errno" },
    { 48, KNOWN_FAILURE, "SO_LINGER set/get roundtrip fails" },
    { 52, KNOWN_FAILURE, "SO_ERROR not set after failed connect" },
    { 63, KNOWN_FAILURE, "WaitSelect NULL fdsets returns immediately" },
    { 69, KNOWN_FAILURE, "WaitSelect nfds not enforced" },
    { 78, KNOWN_FAILURE, "SBTC_DTABLESIZE GET returns 0" },
    { 98, KNOWN_FAILURE, "gethostname() returns empty string" },
    { 111, KNOWN_FAILURE, "Inet_LnaOf() returns 0" },
    { 112, KNOWN_FAILURE, "Inet_NetOf() returns 0" },
    { 113, KNOWN_FAILURE, "Inet_MakeAddr() returns 0 (LnaOf/NetOf broken)" },
    { 116, KNOWN_FAILURE, "Dup2Socket() to specific slot not implemented" },
    { 128, KNOWN_FAILURE, "SBTC_DTABLESIZE GET returns 0" },
};

/* ---- Profile table ---- */

static const struct stack_profile profiles[] = {
    {
        "Roadshow 4.364",
        "Roadshow",
        roadshow_entries,
        sizeof(roadshow_entries) / sizeof(roadshow_entries[0])
    },
    {
        "UAE 8.0.0",
        "Amiberry",
        amiberry_entries,
        sizeof(amiberry_entries) / sizeof(amiberry_entries[0])
    },
    {
        "UAE 6.0.2",
        "WinUAE",
        winuae_entries,
        sizeof(winuae_entries) / sizeof(winuae_entries[0])
    },
};

#define NUM_PROFILES (int)(sizeof(profiles) / sizeof(profiles[0]))

static const struct stack_profile *active_profile;

/* ---- Public API ---- */

void known_init(const char *version_string)
{
    int i;

    active_profile = NULL;

    if (!version_string)
        return;

    for (i = 0; i < NUM_PROFILES; i++) {
        if (strcmp(version_string, profiles[i].match_version) == 0) {
            active_profile = &profiles[i];
            return;
        }
    }
}

/* Internal: search entries with optional type filter.
 * filter_type: -1 = any type, KNOWN_FAILURE or KNOWN_CRASH = specific. */
static const char *lookup(int test_number, int filter_type)
{
    int i;

    if (!active_profile)
        return NULL;

    for (i = 0; i < active_profile->entry_count; i++) {
        if (active_profile->entries[i].test_number != test_number)
            continue;
        if (filter_type >= 0 &&
            (int)active_profile->entries[i].type != filter_type)
            continue;
        return active_profile->entries[i].reason;
    }
    return NULL;
}

const char *known_check(int test_number)
{
    return lookup(test_number, -1);
}

const char *known_crash(int test_number)
{
    return lookup(test_number, KNOWN_CRASH);
}

const char *known_stack_name(void)
{
    if (active_profile)
        return active_profile->stack_name;
    return "Unknown";
}
