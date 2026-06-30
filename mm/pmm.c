#include "pmm.h"
#include "../lib/string.h"
#include <stdint.h>
#include <stdbool.h>

#define MB2_MAGIC_VAL    0x36D76289u
#define TAG_END          0u
#define TAG_MMAP         6u
#define TAG_FRAMEBUFFER  8u

typedef struct __attribute__((packed)) { uint32_t total, reserved; } mb2_hdr_t;
typedef struct __attribute__((packed)) { uint32_t type, size;      } mb2_tag_t;
typedef struct __attribute__((packed)) {
    uint32_t type, size, entry_size, entry_ver;
} mb2_mmap_tag_t;
typedef struct __attribute__((packed)) {
    uint64_t base, len;
    uint32_t type, _res;
} mb2_mmap_entry_t;
typedef struct __attribute__((packed)) {
    uint32_t type, size;
    uint64_t fb_addr;
    uint32_t fb_pitch;
    uint32_t fb_width;
    uint32_t fb_height;
    uint8_t  fb_bpp;
    uint8_t  fb_type;
    uint16_t reserved;
} mb2_fb_tag_t;

#define BITMAP_MAX_PAGES (16 * 1024 * 1024)   

static uint8_t  *bitmap      = NULL;
static size_t    total_pages = 0;
static size_t    free_pages  = 0;
static size_t    bitmap_size = 0;   

static uint64_t   fb_addr = 0;
static uint64_t   fb_len  = 0;

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
        } else if (tag->type == TAG_FRAMEBUFFER) {
            mb2_fb_tag_t *fb = (mb2_fb_tag_t *)ptr;
            fb_addr = fb->fb_addr;
            fb_len  = (uint64_t)fb->fb_pitch * fb->fb_height;
            uint64_t fb_top = fb_addr + fb_len;
            if (fb_top > max_addr) max_addr = fb_top;
        }
        ptr += (tag->size + 7) & ~7u;
    }

    total_pages = (size_t)(max_addr / PAGE_SIZE);
    if (total_pages > BITMAP_MAX_PAGES) total_pages = BITMAP_MAX_PAGES;
    bitmap_size = (total_pages + 7) / 8;

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
                if (e->type == 1 && e->base >= 0x200000 &&
                    e->len >= bitmap_size) {
                    bitmap = (uint8_t *)(uintptr_t)e->base;
                    placed = true;
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

    for (uint64_t a = 0; a < 0x200000 + bitmap_size + PAGE_SIZE; a += PAGE_SIZE) {
        size_t idx = (size_t)(a / PAGE_SIZE);
        if (idx < total_pages && !bm_test(idx)) {
            bm_set(idx);
            free_pages--;
        }
    }

    if (fb_addr != 0 && fb_len != 0) {
        uint64_t start = fb_addr & ~(uint64_t)(PAGE_SIZE - 1);
        uint64_t stop  = (fb_addr + fb_len + PAGE_SIZE - 1) & ~(uint64_t)(PAGE_SIZE - 1);
        for (uint64_t a = start; a < stop; a += PAGE_SIZE) {
            size_t idx = (size_t)(a / PAGE_SIZE);
            if (idx < total_pages && !bm_test(idx)) {
                bm_set(idx);
                free_pages--;
            }
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