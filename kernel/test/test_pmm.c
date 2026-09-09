#include "ktest.h"
#include "../../mm/pmm.h"
#include "../../lib/string.h"
#include <stdint.h>
#include <stddef.h>

void
ktest_pmm(void)
{
    ktest_suite_begin("pmm");

    size_t f0 = pmm_free_pages();

    void *a = pmm_alloc();
    void *b = pmm_alloc();

    KTEST_ASSERT(a != NULL);
    KTEST_ASSERT(b != NULL);
    if (a == NULL || b == NULL) {
        ktest_suite_end();
        return;
    }

    KTEST_ASSERT(a != b);
    KTEST_ASSERT((((uint64_t)(uintptr_t)a) & (PAGE_SIZE - 1)) == 0);
    KTEST_ASSERT((((uint64_t)(uintptr_t)b) & (PAGE_SIZE - 1)) == 0);

    /* pages must be writable and distinct */
    kmemset(a, 0xA5, PAGE_SIZE);
    kmemset(b, 0x5A, PAGE_SIZE);
    KTEST_ASSERT(((uint8_t *)a)[0] == 0xA5);
    KTEST_ASSERT(((uint8_t *)a)[PAGE_SIZE - 1] == 0xA5);
    KTEST_ASSERT(((uint8_t *)b)[0] == 0x5A);
    KTEST_ASSERT(((uint8_t *)b)[PAGE_SIZE - 1] == 0x5A);

    /* relative accounting check (robust to absolute-counter quirks) */
    size_t f1 = pmm_free_pages();
    KTEST_ASSERT(f1 == f0 - 2);

    pmm_free(a);
    pmm_free(b);

    size_t f2 = pmm_free_pages();
    KTEST_ASSERT(f2 == f0);

    ktest_suite_end();
}
