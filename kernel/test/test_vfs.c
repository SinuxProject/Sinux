#include "ktest.h"
#include "../fs/vfs.h"
#include "../../lib/string.h"
#include <stdint.h>
#include <stddef.h>

void
ktest_vfs(void)
{
    ktest_suite_begin("vfs");

    KTEST_ASSERT(vfs_create("/ktest", FT_DIR) == 0);

    file_t *f = vfs_open("/ktest/f", O_WRONLY | O_CREAT | O_TRUNC);
    KTEST_ASSERT(f != NULL);
    if (f == NULL) {
        ktest_suite_end();
        return;
    }

    static const char msg[] = "sinux-ktest-123";
    size_t len = kstrlen(msg);

    int64_t w = vfs_write(f, msg, len);
    KTEST_ASSERT(w == (int64_t)len);
    KTEST_ASSERT(vfs_close(f) == 0);

    f = vfs_open("/ktest/f", O_RDONLY);
    KTEST_ASSERT(f != NULL);
    if (f == NULL) {
        ktest_suite_end();
        return;
    }

    char buf[64];
    kmemset(buf, 0, sizeof(buf));
    int64_t r = vfs_read(f, buf, sizeof(buf) - 1);
    KTEST_ASSERT(r == (int64_t)len);
    KTEST_ASSERT(kstrcmp(buf, msg) == 0);
    KTEST_ASSERT(vfs_close(f) == 0);

    KTEST_ASSERT(vfs_unlink("/ktest/f") == 0);
    KTEST_ASSERT(vfs_open("/ktest/f", O_RDONLY) == NULL);

    ktest_suite_end();
}
