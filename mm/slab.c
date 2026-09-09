#include "slab.h"
#include "pmm.h"
#include "../lib/string.h"
#include "../lib/printk.h"

#define MAX_CACHES 32
#define SLAB_SIZE PAGE_SIZE

static slab_cache_t *caches[MAX_CACHES];
static int num_caches = 0;

static slab_cache_t size_caches[8];

static inline void
bitmap_set(uint64_t *bm, int idx)
{
    bm[idx / 64] |= (1ULL << (idx % 64));
}

static inline void
bitmap_clear(uint64_t *bm, int idx)
{
    bm[idx / 64] &= ~(1ULL << (idx % 64));
}

static inline bool
bitmap_test(uint64_t *bm, int idx)
{
    return (bm[idx / 64] >> (idx % 64)) & 1ULL;
}

static int
bitmap_find_free(uint64_t *bm, int max)
{
    for (int i = 0; i < max; i++) {
        if (!bitmap_test(bm, i))
            return i;
    }
    return -1;
}

static slab_header_t *
slab_create(size_t obj_size)
{
    void *page = pmm_alloc();
    if (!page) return NULL;

    slab_header_t *slab = (slab_header_t *)page;
    kmemset(slab, 0, sizeof(slab_header_t));
    
    slab->magic = SLAB_MAGIC;
    slab->obj_size = obj_size;
    
    size_t usable = SLAB_SIZE - sizeof(slab_header_t);
    slab->total_objs = usable / obj_size;
    if (slab->total_objs > SLAB_MAX_OBJS)
        slab->total_objs = SLAB_MAX_OBJS;
    
    slab->free_objs = slab->total_objs;
    slab->next = NULL;
    
    kmemset(slab->free_bitmap, 0, sizeof(slab->free_bitmap));
    
    return slab;
}

static void *
slab_get_obj(slab_header_t *slab, int idx)
{
    uint8_t *base = (uint8_t *)slab + sizeof(slab_header_t);
    return base + (idx * slab->obj_size);
}

static int
slab_find_obj_idx(slab_header_t *slab, void *ptr)
{
    uint8_t *base = (uint8_t *)slab + sizeof(slab_header_t);
    uint8_t *p = (uint8_t *)ptr;
    
    if (p < base) return -1;
    
    ptrdiff_t offset = p - base;
    if (offset % slab->obj_size != 0) return -1;
    
    int idx = offset / slab->obj_size;
    if (idx >= (int)slab->total_objs) return -1;
    
    return idx;
}

slab_cache_t *
slab_cache_create(const char *name, size_t size, size_t align)
{
    if (num_caches >= MAX_CACHES) return NULL;
    if (size == 0 || size > SLAB_SIZE / 2) return NULL;
    
    size_t aligned_size = (size + align - 1) & ~(align - 1);
    if (aligned_size < 16) aligned_size = 16;
    
    slab_cache_t *cache = (slab_cache_t *)kmalloc_slab(sizeof(slab_cache_t));
    if (!cache) return NULL;
    
    kmemset(cache, 0, sizeof(slab_cache_t));
    cache->name = name;
    cache->obj_size = aligned_size;
    cache->align = align;
    cache->partial = NULL;
    cache->full = NULL;
    cache->num_allocs = 0;
    cache->num_frees = 0;
    cache->num_slabs = 0;
    
    caches[num_caches++] = cache;
    return cache;
}

void *
slab_alloc(slab_cache_t *cache)
{
    if (!cache) return NULL;
    
    slab_header_t *slab = cache->partial;
    
    if (!slab) {
        slab = slab_create(cache->obj_size);
        if (!slab) return NULL;
        
        slab->next = cache->partial;
        cache->partial = slab;
        cache->num_slabs++;
    }
    
    int idx = bitmap_find_free(slab->free_bitmap, slab->total_objs);
    if (idx < 0) {
        slab = slab_create(cache->obj_size);
        if (!slab) return NULL;
        
        slab->next = cache->partial;
        cache->partial = slab;
        cache->num_slabs++;
        idx = 0;
    }
    
    bitmap_set(slab->free_bitmap, idx);
    slab->free_objs--;
    cache->num_allocs++;
    
    if (slab->free_objs == 0) {
        if (cache->partial == slab) {
            cache->partial = slab->next;
        } else {
            slab_header_t *prev = cache->partial;
            while (prev && prev->next != slab) prev = prev->next;
            if (prev) prev->next = slab->next;
        }
        slab->next = cache->full;
        cache->full = slab;
    }
    
    return slab_get_obj(slab, idx);
}

void
slab_free(slab_cache_t *cache, void *ptr)
{
    if (!cache || !ptr) return;
    
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t slab_addr = addr & ~(SLAB_SIZE - 1);
    slab_header_t *slab = (slab_header_t *)slab_addr;
    
    if (slab->magic != SLAB_MAGIC) return;
    
    int idx = slab_find_obj_idx(slab, ptr);
    if (idx < 0 || !bitmap_test(slab->free_bitmap, idx)) return;
    
    bool was_full = (slab->free_objs == 0);
    
    bitmap_clear(slab->free_bitmap, idx);
    slab->free_objs++;
    cache->num_frees++;
    
    if (was_full) {
        if (cache->full == slab) {
            cache->full = slab->next;
        } else {
            slab_header_t *prev = cache->full;
            while (prev && prev->next != slab) prev = prev->next;
            if (prev) prev->next = slab->next;
        }
        slab->next = cache->partial;
        cache->partial = slab;
    }
    
    if (slab->free_objs == slab->total_objs && cache->num_slabs > 2) {
        if (cache->partial == slab) {
            cache->partial = slab->next;
        } else {
            slab_header_t *prev = cache->partial;
            while (prev && prev->next != slab) prev = prev->next;
            if (prev) prev->next = slab->next;
        }
        cache->num_slabs--;
        pmm_free(slab);
    }
}

void
slab_cache_destroy(slab_cache_t *cache)
{
    if (!cache) return;
    
    slab_header_t *slab = cache->partial;
    while (slab) {
        slab_header_t *next = slab->next;
        pmm_free(slab);
        slab = next;
    }
    
    slab = cache->full;
    while (slab) {
        slab_header_t *next = slab->next;
        pmm_free(slab);
        slab = next;
    }
    
    for (int i = 0; i < num_caches; i++) {
        if (caches[i] == cache) {
            caches[i] = caches[--num_caches];
            break;
        }
    }
    
    kfree_slab(cache);
}

void
slab_init(void)
{
    size_t sizes[] = { 16, 32, 64, 128, 256, 512, 1024, 2048 };
    const char *names[] = {
        "kmalloc-16", "kmalloc-32", "kmalloc-64", "kmalloc-128",
        "kmalloc-256", "kmalloc-512", "kmalloc-1024", "kmalloc-2048"
    };
    
    for (int i = 0; i < 8; i++) {
        kmemset(&size_caches[i], 0, sizeof(slab_cache_t));
        size_caches[i].name = names[i];
        size_caches[i].obj_size = sizes[i];
        size_caches[i].align = 16;
    }
    
    printk("[INFO] slab: initialized 8 size caches\n");
}

void *
kmalloc_slab(size_t size)
{
    if (size == 0) return NULL;
    if (size > 2048) {
        size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        void *ptr = pmm_alloc();
        if (!ptr) return NULL;
        for (size_t i = 1; i < pages; i++) {
            void *p = pmm_alloc();
            if (!p) {
                for (size_t j = 0; j < i; j++)
                    pmm_free((void *)((uintptr_t)ptr + j * PAGE_SIZE));
                return NULL;
            }
        }
        return ptr;
    }
    
    for (int i = 0; i < 8; i++) {
        if (size <= size_caches[i].obj_size)
            return slab_alloc(&size_caches[i]);
    }
    
    return NULL;
}

void
kfree_slab(void *ptr)
{
    if (!ptr) return;
    
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t slab_addr = addr & ~(SLAB_SIZE - 1);
    slab_header_t *slab = (slab_header_t *)slab_addr;
    
    if (slab->magic != SLAB_MAGIC) {
        pmm_free(ptr);
        return;
    }
    
    for (int i = 0; i < 8; i++) {
        if (size_caches[i].obj_size == slab->obj_size) {
            slab_free(&size_caches[i], ptr);
            return;
        }
    }
    
    for (int i = 0; i < num_caches; i++) {
        if (caches[i]->obj_size == slab->obj_size) {
            slab_free(caches[i], ptr);
            return;
        }
    }
}
