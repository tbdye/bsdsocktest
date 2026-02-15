/*
 * bsdsocktest — Known failure detection per TCP/IP stack
 *
 * Auto-detects the running stack from SBTC_RELEASESTRPTR and looks up
 * known test failures in a per-stack table. Tests report pass/fail
 * normally; the framework marks matching failures as "known limitation"
 * rather than "unexpected failure".
 */

#include "known_failures.h"

#include <stddef.h>
#include <string.h>

struct known_entry {
    int test_number;
    const char *reason;
};

struct stack_profile {
    const char *match_prefix;    /* prefix match against version string */
    const char *stack_name;      /* display name */
    const struct known_entry *entries;
    int entry_count;
};

/* Roadshow known limitations (verified against Roadshow 4.364) */
static const struct known_entry roadshow_known[] = {
    { 27, "recv(MSG_OOB) returns EINVAL" },
    { 35, "loopback does not generate RST for closed peer" },
    { 76, "SBTC_ERRNOLONGPTR GET not supported (SET-only)" },
    { 77, "SBTC_HERRNOLONGPTR GET not supported (SET-only)" },
};

static const struct stack_profile profiles[] = {
    {
        "Roadshow",
        "Roadshow",
        roadshow_known,
        sizeof(roadshow_known) / sizeof(roadshow_known[0])
    },
    /* Future: Miami, AmiTCP, Genesis, Amiberry emulation */
};

#define NUM_PROFILES (int)(sizeof(profiles) / sizeof(profiles[0]))

static const struct stack_profile *active_profile;

void known_init(const char *version_string)
{
    int i;

    active_profile = NULL;
    if (!version_string)
        return;

    for (i = 0; i < NUM_PROFILES; i++) {
        if (strncmp(version_string, profiles[i].match_prefix,
                    strlen(profiles[i].match_prefix)) == 0) {
            active_profile = &profiles[i];
            return;
        }
    }
}

const char *known_check(int test_number)
{
    int i;

    if (!active_profile)
        return NULL;

    for (i = 0; i < active_profile->entry_count; i++) {
        if (active_profile->entries[i].test_number == test_number)
            return active_profile->entries[i].reason;
    }
    return NULL;
}

const char *known_stack_name(void)
{
    if (active_profile)
        return active_profile->stack_name;
    return "Unknown";
}
