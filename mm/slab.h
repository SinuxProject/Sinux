#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define SLAB_MAGIC 0x5AB0BEEF

/* Max objects per slab: 256 (covers the smallest cache: 16-byte objs
 * in a 4 KiB page). The header must stay small — an oversized bitmap
 * ate the whole page, yielding total_objs=0 and pushing every object
 * past the page end into the next physical page. */
#define SLAB_MAX_OBJS 256

typedef struct slab_header {
    uint32_t magic;
    uint32_t obj_size;
    uint32_t total_objs;
    uint32_t free_objs;
    uint64_t free_bitmap[SLAB_MAX_OBJS / 64];
    struct slab_header *next;
} slab_header_t;

typedef struct {
    const char *name;
    size_t obj_size;
    size_t align;
    slab_header_t *partial;
    slab_header_t *full;
    uint64_t num_allocs;
    uint64_t num_frees;
    uint64_t num_slabs;
} slab_cache_t;

void  slab_init(void);
void *slab_alloc(slab_cache_t *cache);
void  slab_free(slab_cache_t *cache, void *ptr);

slab_cache_t *slab_cache_create(const char *name, size_t size, size_t align);
void          slab_cache_destroy(slab_cache_t *cache);

void *kmalloc_slab(size_t size);
void  kfree_slab(void *ptr);
