/*
 * bsdsocktest — Error handling tests
 *
 * Tests: Errno(), SetErrnoPtr (byte/word/long), errno variable update.
 *
 * 6 tests, no port offsets needed (test 59 borrows offset 0).
 */

#include "tap.h"
#include "testutil.h"

#include <proto/bsdsocket.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

void run_errno_tests(void)
{
    LONG fd;
    LONG errno_val;
    struct sockaddr_in addr;

    /* 54. errno_after_error */
    fd = socket(-1, -1, -1); /* Guaranteed to fail */
    errno_val = Errno();
    tap_ok(fd < 0 && errno_val != 0 && errno_val == get_bsd_errno(),
           "errno_after_error - Errno() matches get_bsd_errno() after failure");
    tap_diagf("  Errno()=%ld, get_bsd_errno()=%ld",
              (long)errno_val, (long)get_bsd_errno());
    if (fd >= 0)
        safe_close(fd);

    CHECK_CTRLC();

    /* 55. errno_after_success — behavioral documentation test.
     * BSD does NOT guarantee errno is cleared on success. */
    CloseSocket(-1); /* Set errno to something non-zero */
    fd = socket(AF_INET, SOCK_STREAM, 0); /* Should succeed */
    if (fd >= 0) {
        errno_val = Errno();
        if (errno_val == 0) {
            tap_ok(1, "errno_after_success - Errno() cleared on success");
            tap_diag("  behavior: errno cleared on success");
        } else {
            tap_ok(1, "errno_after_success - Errno() not cleared on success (BSD-standard)");
            tap_diagf("  behavior: errno=%ld after successful socket()", (long)errno_val);
        }
        safe_close(fd);
    } else {
        tap_ok(0, "errno_after_success - socket() unexpectedly failed");
    }

    CHECK_CTRLC();

    /* 56. seterrnoptr_byte */
    {
        BYTE err_byte = 0;
        SetErrnoPtr(&err_byte, 1);
        CloseSocket(-1);
        tap_ok(err_byte != 0,
               "seterrnoptr_byte - SetErrnoPtr(1) byte updated on error");
        tap_diagf("  byte errno: %d", (int)err_byte);
        restore_bsd_errno();
    }

    CHECK_CTRLC();

    /* 57. seterrnoptr_word */
    {
        WORD err_word = 0;
        SetErrnoPtr(&err_word, 2);
        CloseSocket(-1);
        tap_ok(err_word != 0,
               "seterrnoptr_word - SetErrnoPtr(2) word updated on error");
        tap_diagf("  word errno: %d", (int)err_word);
        restore_bsd_errno();
    }

    CHECK_CTRLC();

    /* 58. seterrnoptr_long */
    {
        LONG err_long = 0;
        SetErrnoPtr(&err_long, 4);
        CloseSocket(-1);
        tap_ok(err_long != 0,
               "seterrnoptr_long - SetErrnoPtr(4) long updated on error");
        tap_diagf("  long errno: %ld", (long)err_long);
        restore_bsd_errno();
    }

    CHECK_CTRLC();

    /* 59. errno_variable_updated — register a fresh variable,
     * do two different failing ops, verify both update it. */
    {
        LONG test_var = 0;
        LONG first_val, second_val;

        SocketBaseTags(
            SBTM_SETVAL(SBTC_ERRNOLONGPTR), (ULONG)&test_var,
            TAG_DONE);

        /* First error: invalid fd */
        CloseSocket(-1);
        first_val = test_var;

        /* Second error: connect to non-listening port */
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd >= 0) {
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(get_test_port(0));
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            connect(fd, (struct sockaddr *)&addr, sizeof(addr));
            second_val = test_var;
            safe_close(fd);

            tap_ok(first_val != 0 && second_val != 0 && first_val != second_val,
                   "errno_variable_updated - registered variable tracks different errors");
            tap_diagf("  first=%ld (expected EBADF=9), second=%ld (expected ECONNREFUSED=61)",
                      (long)first_val, (long)second_val);
        } else {
            tap_ok(first_val != 0,
                   "errno_variable_updated - at least first error recorded");
            tap_diagf("  first=%ld, socket() failed for second test",
                      (long)first_val);
        }

        restore_bsd_errno();
    }
}
