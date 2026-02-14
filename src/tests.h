/*
 * bsdsocktest — Test category function declarations
 *
 * Each test_*.c file implements one of these. They are called from
 * the category dispatch table in main.c.
 */

#ifndef BSDSOCKTEST_TESTS_H
#define BSDSOCKTEST_TESTS_H

void run_socket_tests(void);
void run_sendrecv_tests(void);
void run_sockopt_tests(void);
void run_waitselect_tests(void);
void run_signals_tests(void);
void run_dns_tests(void);
void run_utility_tests(void);
void run_transfer_tests(void);
void run_errno_tests(void);
void run_misc_tests(void);
void run_icmp_tests(void);
void run_throughput_tests(void);

#endif /* BSDSOCKTEST_TESTS_H */
