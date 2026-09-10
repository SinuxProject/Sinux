#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Spinlock Implementation ────────────────────────────────────── */
typedef struct {
    volatile uint32_t locked;
    volatile int owner_cpu;
    const char *name;
    uint64_t acquire_time;
} spinlock_t;

#define SPINLOCK_INIT(name) { .locked = 0, .owner_cpu = -1, .name = #name }

void spinlock_init(spinlock_t *lock, const char *name);
void spinlock_acquire(spinlock_t *lock);
void spinlock_release(spinlock_t *lock);
bool spinlock_try_acquire(spinlock_t *lock);

/* ── Mutex Implementation (with sleeping support) ───────────────── */
typedef struct {
    volatile uint32_t count;
    volatile int owner_cpu;
    void *wait_queue;
    const char *name;
} mutex_t;

#define MUTEX_INIT(name) { .count = 1, .owner_cpu = -1, .wait_queue = NULL, .name = #name }

void mutex_init(mutex_t *lock, const char *name);
void mutex_acquire(mutex_t *lock);
void mutex_release(mutex_t *lock);

/* ── Read-Write Lock ────────────────────────────────────────────── */
typedef struct {
    volatile uint32_t readers;
    volatile uint32_t writers;
    volatile int writer_cpu;
    const char *name;
} rwlock_t;

#define RWLOCK_INIT(name) { .readers = 0, .writers = 0, .writer_cpu = -1, .name = #name }

void rwlock_init(rwlock_t *lock, const char *name);
void rwlock_read_acquire(rwlock_t *lock);
void rwlock_read_release(rwlock_t *lock);
void rwlock_write_acquire(rwlock_t *lock);
void rwlock_write_release(rwlock_t *lock);

/* ── Atomic Operations ──────────────────────────────────────────── */
static inline int atomic_inc(volatile int *ptr) {
    int old;
    __asm__ volatile("lock xaddl %0, %1" 
        : "=r"(old), "+m"(*ptr) 
        : "0"(1) 
        : "memory", "cc");
    return old;
}

static inline int atomic_dec(volatile int *ptr) {
    int old;
    __asm__ volatile("lock xaddl %0, %1" 
        : "=r"(old), "+m"(*ptr) 
        : "0"(-1) 
        : "memory", "cc");
    return old;
}

static inline int atomic_add(volatile int *ptr, int val) {
    int old;
    __asm__ volatile("lock xaddl %0, %1" 
        : "=r"(old), "+m"(*ptr) 
        : "0"(val) 
        : "memory", "cc");
    return old;
}

static inline int atomic_cmpxchg(volatile int *ptr, int expected, int desired) {
    int old;
    __asm__ volatile("lock cmpxchgl %2, %1"
        : "=a"(old), "+m"(*ptr)
        : "r"(desired), "0"(expected)
        : "memory", "cc");
    return old;
}

static inline uint64_t atomic_inc64(volatile uint64_t *ptr) {
    uint64_t old;
    __asm__ volatile("lock xaddq %0, %1" 
        : "=r"(old), "+m"(*ptr) 
        : "0"(1) 
        : "memory", "cc");
    return old;
}

static inline uint64_t atomic_dec64(volatile uint64_t *ptr) {
    uint64_t old;
    __asm__ volatile("lock xaddq %0, %1" 
        : "=r"(old), "+m"(*ptr) 
        : "0"((uint64_t)-1) 
        : "memory", "cc");
    return old;
}

/* Memory Barriers */
#define mb()  __asm__ volatile("mfence" ::: "memory")
#define rmb() __asm__ volatile("lfence" ::: "memory")
#define wmb() __asm__ volatile("sfence" ::: "memory")
#define barrier() __asm__ volatile("" ::: "memory")

/* ── TLB Shootdown ──────────────────────────────────────────────── */
void tlb_shootdown(uint64_t *pml4, uint64_t virt_addr);
void tlb_shootdown_range(uint64_t *pml4, uint64_t start, uint64_t end);
void tlb_flush_local(void);
void tlb_flush_global(void);

/* ── Simple Deadlock Detection (lockdep-lite) ───────────────────── */
void lockdep_init(void);
void lockdep_acquire(void *lock_addr, const char *lock_name);
void lockdep_release(void *lock_addr);
void lockdep_check_deadlock(void);
