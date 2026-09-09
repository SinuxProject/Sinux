#include "ktest.h"
#include "../../lib/printk.h"
#include <stdint.h>

static const char *cur_suite = "?";
static uint64_t suite_pass = 0, suite_total = 0;
static uint64_t suites_run = 0, suites_failed = 0;
static uint64_t asserts_pass = 0, asserts_total = 0;

void ktest_pmm(void);
void ktest_vfs(void);

void
ktest_suite_begin(const char *name)
{
    cur_suite   = name;
    suite_pass  = 0;
    suite_total = 0;
    suites_run++;
    printk("ktest: --- suite: %s ---\n", name);
}

void
ktest_assert_impl(int cond, const char *expr,
                  const char *file, int line)
{
    asserts_total++;
    suite_total++;
    if (cond) {
        asserts_pass++;
        suite_pass++;
    } else {
        /* NOTE: printk %u/%d read 64-bit slots — always cast. */
        printk("ktest: ASSERT FAILED: %s (%s:%u)\n",
               expr, file, (uint64_t)line);
    }
}

void
ktest_suite_end(void)
{
    if (suite_pass == suite_total) {
        printk("ktest: PASS %s (%u/%u asserts)\n",
               cur_suite, suite_pass, suite_total);
    } else {
        suites_failed++;
        printk("ktest: FAIL %s (%u/%u asserts passed)\n",
               cur_suite, suite_pass, suite_total);
    }
}

void
ktest_run_all(void)
{
    printk("ktest: running kernel self-tests...\n");
    ktest_pmm();
    ktest_vfs();
    if (suites_failed == 0) {
        printk("ktest: ALL TESTS PASSED (%u/%u suites, %u/%u asserts)\n",
               suites_run, suites_run, asserts_pass, asserts_total);
    } else {
        printk("ktest: FAILURES: %u/%u suites failed "
               "(%u/%u asserts passed)\n",
               suites_failed, suites_run, asserts_pass, asserts_total);
    }
}

int
ktest_failed(void)
{
    return suites_failed != 0;
}
