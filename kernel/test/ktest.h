#pragma once

/*
 * Minimal in-kernel unit-test framework (Phase 0).
 *
 * Suites run at boot via ktest_run_all() and report over serial
 * through printk. The host-side smoke test judges the result by
 * asserting on the summary line "ktest: ALL TESTS PASSED".
 */

void ktest_run_all(void);
int  ktest_failed(void);   /* nonzero if any assert failed */

void ktest_suite_begin(const char *name);
void ktest_suite_end(void);
void ktest_assert_impl(int cond, const char *expr,
                       const char *file, int line);

#define KTEST_ASSERT(cond) \
    ktest_assert_impl((cond) ? 1 : 0, #cond, __FILE__, __LINE__)
