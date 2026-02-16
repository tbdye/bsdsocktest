/*
 * bsdsocktest — Known failure and crash detection per TCP/IP stack
 *
 * Auto-detects the running stack from SBTC_RELEASESTRPTR, parses the
 * version number, and looks up known issues in a per-stack table.
 *
 * Two types of entries, both version-gated:
 *   KNOWN_FAILURE — test runs and fails; framework annotates as "known"
 *   KNOWN_CRASH   — test would crash the emulator; must be skipped
 *
 * Version gating: entries apply only when the detected version is <=
 * the profile's max_version. When a stack ships a fix and bumps its
 * version, the guards stop applying automatically — no code changes.
 */

#include "known_failures.h"

#include <stddef.h>
#include <string.h>

/* ---- Version representation ---- */

struct stack_version {
    int major;
    int minor;
    int patch;
};

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
    const char *match_prefix;       /* prefix match against version string */
    const char *stack_name;         /* display name */
    struct stack_version max_version; /* entries apply to versions <= this */
    const struct known_entry *entries;
    int entry_count;
};

/* ---- Roadshow (verified against 4.364 and 4.347) ---- */

static const struct known_entry roadshow_entries[] = {
    { 27, KNOWN_FAILURE, "recv(MSG_OOB) returns EINVAL" },
    { 35, KNOWN_FAILURE, "loopback does not generate RST for closed peer" },
    { 76, KNOWN_FAILURE, "SBTC_ERRNOLONGPTR GET not supported (SET-only)" },
    { 77, KNOWN_FAILURE, "SBTC_HERRNOLONGPTR GET not supported (SET-only)" },
};

/* ---- UAE / Amiberry bsdsocket emulation (verified against 8.0.0) ---- */

static const struct known_entry uae_entries[] = {
    /* Crashes: exercising these operations causes exit(1) */
    { 70, KNOWN_CRASH,   "WaitSelect >64 fds crashes emulator" },
    { 79, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    { 80, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    { 81, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    { 82, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    { 83, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    { 84, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    { 85, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    { 87, KNOWN_CRASH,   "SO_EVENTMASK crashes emulator" },
    /* Failures: tests run but produce wrong results */
    { 31, KNOWN_FAILURE, "sendmsg() not implemented" },
    { 32, KNOWN_FAILURE, "recvmsg() not implemented" },
    { 49, KNOWN_FAILURE, "SO_RCVTIMEO set/get roundtrip fails" },
    { 50, KNOWN_FAILURE, "SO_SNDTIMEO set/get roundtrip fails" },
    { 63, KNOWN_FAILURE, "WaitSelect NULL fdsets returns immediately" },
    { 78, KNOWN_FAILURE, "SBTC_DTABLESIZE GET returns 0" },
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

/* ---- Profile table ---- */

static const struct stack_profile profiles[] = {
    {
        "Roadshow",
        "Roadshow",
        { 4, 364, 0 },
        roadshow_entries,
        sizeof(roadshow_entries) / sizeof(roadshow_entries[0])
    },
    {
        "UAE",
        "UAE",
        { 8, 0, 0 },
        uae_entries,
        sizeof(uae_entries) / sizeof(uae_entries[0])
    },
};

#define NUM_PROFILES (int)(sizeof(profiles) / sizeof(profiles[0]))

static const struct stack_profile *active_profile;
static struct stack_version detected_version;

/* ---- Version parsing ---- */

/* Parse "X.Y.Z", "X.Y", or "X" from a string (after the prefix).
 * Skips leading whitespace.  Returns number of components parsed. */
static int parse_version(const char *s, struct stack_version *v)
{
    int components = 0;
    int val = 0;
    int has_digit = 0;

    v->major = 0;
    v->minor = 0;
    v->patch = 0;

    while (*s == ' ')
        s++;

    while (*s) {
        if (*s >= '0' && *s <= '9') {
            val = val * 10 + (*s - '0');
            has_digit = 1;
        } else if (*s == '.' && has_digit) {
            if (components == 0)      v->major = val;
            else if (components == 1) v->minor = val;
            else break;
            components++;
            val = 0;
            has_digit = 0;
        } else {
            break;
        }
        s++;
    }

    if (has_digit) {
        if (components == 0)      v->major = val;
        else if (components == 1) v->minor = val;
        else if (components == 2) v->patch = val;
        components++;
    }

    return components;
}

/* Return non-zero if a <= b (lexicographic comparison). */
static int version_lte(const struct stack_version *a,
                       const struct stack_version *b)
{
    if (a->major != b->major) return a->major < b->major;
    if (a->minor != b->minor) return a->minor < b->minor;
    return a->patch <= b->patch;
}

/* ---- Public API ---- */

void known_init(const char *version_string)
{
    int i;
    size_t prefix_len;

    active_profile = NULL;
    detected_version.major = 0;
    detected_version.minor = 0;
    detected_version.patch = 0;

    if (!version_string)
        return;

    for (i = 0; i < NUM_PROFILES; i++) {
        prefix_len = strlen(profiles[i].match_prefix);
        if (strncmp(version_string, profiles[i].match_prefix,
                    prefix_len) == 0) {
            active_profile = &profiles[i];
            parse_version(version_string + prefix_len, &detected_version);
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

    /* Version gate: entries only apply to versions <= max_version */
    if (!version_lte(&detected_version, &active_profile->max_version))
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
