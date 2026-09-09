
#include "pmm.h"
#include "../lib/string.h"
#include <stdint.h>
#include <stdbool.h>

#define MB2_MAGIC_VAL  0x36D76289u
#define TAG_END        0u
#define TAG_MMAP       6u

typedef struct __attribute__((packed)) { uint32_t total, reserved; } mb2_hdr_t;
typedef struct __attribute__((packed)) { uint32_t type, size;      } mb2_tag_t;
typedef struct __attribute__((packed)) {
    uint32_t type, size, entry_size, entry_ver;
} mb2_mmap_tag_t;
typedef struct __attribute__((packed)) {
    uint64_t base, len;
    uint32_t type, _res;
} mb2_mmap_entry_t;

#define BITMAP_MAX_PAGES (16 * 1024 * 1024)

/* Physical end of the kernel image (linker-provided; the image is
 * identity-mapped at boot). Everything below it is reserved and must
 * never be handed out as free pages or heap. */
extern uint64_t _kernel_end[];

static uint8_t  *bitmap      = NULL;
static size_t    total_pages = 0;
static size_t    free_pages  = 0;
static size_t    bitmap_size = 0;   

static void   bm_set  (size_t idx) { bitmap[idx/8] |=  (uint8_t)(1u << (idx%8)); }
static void   bm_clear(size_t idx) { bitmap[idx/8] &= (uint8_t)~(1u << (idx%8)); }
static bool   bm_test (size_t idx) { return (bitmap[idx/8] >> (idx%8)) & 1u; }

void
pmm_init(uint32_t magic, uint64_t info_addr)
{
    if (magic != MB2_MAGIC_VAL) return;

    mb2_hdr_t *hdr = (mb2_hdr_t *)(uintptr_t)info_addr;
    uint8_t   *ptr = (uint8_t *)hdr + 8;
    uint8_t   *end = (uint8_t *)hdr + hdr->total;

    uint64_t max_addr = 0;
    while (ptr + sizeof(mb2_tag_t) <= end) {
        mb2_tag_t *tag = (mb2_tag_t *)ptr;
        if (tag->type == TAG_END) break;
        if (tag->type == TAG_MMAP) {
            mb2_mmap_tag_t *mt = (mb2_mmap_tag_t *)ptr;
            uint8_t *ep = ptr + sizeof(mb2_mmap_tag_t);
            uint8_t *ee = ptr + tag->size;
            while (ep + mt->entry_size <= ee) {
                mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)ep;
                uint64_t top = e->base + e->len;
                if (e->type == 1 && top > max_addr) max_addr = top;
                ep += mt->entry_size;
            }
        }
        ptr += (tag->size + 7) & ~7u;
    }

    total_pages = (size_t)(max_addr / PAGE_SIZE);
    if (total_pages > BITMAP_MAX_PAGES) total_pages = BITMAP_MAX_PAGES;
    bitmap_size = (total_pages + 7) / 8;

    uint64_t kernel_end =
        ((uint64_t)_kernel_end + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);

    ptr = (uint8_t *)hdr + 8;
    bool placed = false;
    while (ptr + sizeof(mb2_tag_t) <= end && !placed) {
        mb2_tag_t *tag = (mb2_tag_t *)ptr;
        if (tag->type == TAG_END) break;
        if (tag->type == TAG_MMAP) {
            mb2_mmap_tag_t *mt = (mb2_mmap_tag_t *)ptr;
            uint8_t *ep = ptr + sizeof(mb2_mmap_tag_t);
            uint8_t *ee = ptr + tag->size;
            while (ep + mt->entry_size <= ee && !placed) {
                mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)ep;
                if (e->type == 1) {
                    /* place the bitmap above the kernel image,
                     * inside a region with enough room */
                    uint64_t place =
                        e->base < kernel_end ? kernel_end : e->base;
                    place = (place + PAGE_SIZE - 1) &
                            ~(uint64_t)(PAGE_SIZE - 1);
                    if (place + bitmap_size <= e->base + e->len) {
                        bitmap = (uint8_t *)(uintptr_t)place;
                        placed = true;
                    }
                }
                ep += mt->entry_size;
            }
        }
        ptr += (tag->size + 7) & ~7u;
    }
    if (!bitmap) return;

    kmemset(bitmap, 0xFF, bitmap_size);
    free_pages = 0;

    ptr = (uint8_t *)hdr + 8;
    while (ptr + sizeof(mb2_tag_t) <= end) {
        mb2_tag_t *tag = (mb2_tag_t *)ptr;
        if (tag->type == TAG_END) break;
        if (tag->type == TAG_MMAP) {
            mb2_mmap_tag_t *mt = (mb2_mmap_tag_t *)ptr;
            uint8_t *ep = ptr + sizeof(mb2_mmap_tag_t);
            uint8_t *ee = ptr + tag->size;
            while (ep + mt->entry_size <= ee) {
                mb2_mmap_entry_t *e = (mb2_mmap_entry_t *)ep;
                if (e->type == 1) {
                    uint64_t start = (e->base + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE-1);
                    uint64_t stop  = (e->base + e->len) & ~(uint64_t)(PAGE_SIZE-1);
                    for (uint64_t a = start; a < stop; a += PAGE_SIZE) {
                        size_t idx = (size_t)(a / PAGE_SIZE);
                        if (idx < total_pages) { bm_clear(idx); free_pages++; }
                    }
                }
                ep += mt->entry_size;
            }
        }
        ptr += (tag->size + 7) & ~7u;
    }

    /* reserve [0, max(kernel_end, bitmap_end)): neither the kernel
     * image nor the bitmap itself may ever be handed out as free pages */
    uint64_t reserve_end = kernel_end;
    uint64_t bitmap_end =
        ((uint64_t)bitmap + bitmap_size + PAGE_SIZE - 1) &
        ~(uint64_t)(PAGE_SIZE - 1);
    if (bitmap_end > reserve_end)
        reserve_end = bitmap_end;

    for (uint64_t a = 0; a < reserve_end; a += PAGE_SIZE) {
        size_t idx = (size_t)(a / PAGE_SIZE);
        if (idx < total_pages && !bm_test(idx)) {
            bm_set(idx);
            free_pages--;
        }
    }
}

void *
pmm_alloc(void)
{
    for (size_t i = 0; i < total_pages; i++) {
        if (!bm_test(i)) {
            bm_set(i);
            free_pages--;
            return (void *)(uintptr_t)(i * PAGE_SIZE);
        }
    }
    return NULL;  
}

void
pmm_free(void *page)
{
    size_t idx = (uintptr_t)page / PAGE_SIZE;
    if (idx < total_pages && bm_test(idx)) {
        bm_clear(idx);
        free_pages++;
    }
}

size_t pmm_free_pages(void)  { return free_pages;  }
size_t pmm_total_pages(void) { return total_pages; }
