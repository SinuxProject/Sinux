#include "lock.h"
#include "cpu.h"
#include "../../lib/printk.h"
#include "../../lib/string.h"

/* ── Spinlock Implementation ────────────────────────────────────── */

void spinlock_init(spinlock_t *lock, const char *name) {
    lock->locked = 0;
    lock->owner_cpu = -1;
    lock->name = name;
    lock->acquire_time = 0;
}

void spinlock_acquire(spinlock_t *lock) {
    int cpu_id = get_current_cpu_id();
    
    /* Fast path: try atomic exchange */
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        /* Spin with PAUSE instruction to reduce power and improve performance */
        __asm__ volatile("pause");
    }
    
    /* We acquired the lock */
    lock->owner_cpu = cpu_id;
    lock->acquire_time = rdtsc();
    
    /* Memory barrier to ensure all previous writes are visible */
    mb();
}

void spinlock_release(spinlock_t *lock) {
    /* Memory barrier before releasing */
    mb();
    
    lock->owner_cpu = -1;
    lock->acquire_time = 0;
    
    /* Release the lock */
    __sync_lock_release(&lock->locked);
}

bool spinlock_try_acquire(spinlock_t *lock) {
    if (!__sync_lock_test_and_set(&lock->locked, 1)) {
        lock->owner_cpu = get_current_cpu_id();
        lock->acquire_time = rdtsc();
        mb();
        return true;
    }
    return false;
}

/* ── Mutex Implementation ───────────────────────────────────────── */

void mutex_init(mutex_t *lock, const char *name) {
    lock->count = 1;
    lock->owner_cpu = -1;
    lock->wait_queue = NULL;
    lock->name = name;
}

void mutex_acquire(mutex_t *lock) {
    /* Try fast path first */
    if (atomic_cmpxchg((volatile int *)&lock->count, 1, 0) == 1) {
        lock->owner_cpu = get_current_cpu_id();
        return;
    }
    
    /* Slow path: spin until available */
    for (;;) {
        if (lock->count == 1 && 
            atomic_cmpxchg((volatile int *)&lock->count, 1, 0) == 1) {
            lock->owner_cpu = get_current_cpu_id();
            return;
        }
        __asm__ volatile("pause");
    }
}

void mutex_release(mutex_t *lock) {
    mb();
    lock->owner_cpu = -1;
    lock->count = 1;
}

/* ── Read-Write Lock Implementation ─────────────────────────────── */

void rwlock_init(rwlock_t *lock, const char *name) {
    lock->readers = 0;
    lock->writers = 0;
    lock->writer_cpu = -1;
    lock->name = name;
}

void rwlock_read_acquire(rwlock_t *lock) {
    /* Wait for any writer to finish */
    while (lock->writers > 0) {
        __asm__ volatile("pause");
    }
    
    /* Increment reader count atomically */
    atomic_inc((volatile int *)&lock->readers);
    
    /* Double-check no writer started while we were incrementing */
    if (lock->writers > 0) {
        atomic_dec((volatile int *)&lock->readers);
        rwlock_read_acquire(lock);  /* Retry */
    }
}

void rwlock_read_release(rwlock_t *lock) {
    mb();
    atomic_dec((volatile int *)&lock->readers);
}

void rwlock_write_acquire(rwlock_t *lock) {
    /* Acquire exclusive access */
    while (__sync_lock_test_and_set(&lock->writers, 1)) {
        __asm__ volatile("pause");
    }
    
    /* Wait for all readers to finish */
    while (lock->readers > 0) {
        __asm__ volatile("pause");
    }
    
    lock->writer_cpu = get_current_cpu_id();
    mb();
}

void rwlock_write_release(rwlock_t *lock) {
    mb();
    lock->writer_cpu = -1;
    lock->writers = 0;
}

/* ── TLB Shootdown Implementation ───────────────────────────────── */

void tlb_flush_local(void) {
    __asm__ volatile("invlpg (%0)" :: "r"(0) : "memory");
}

void tlb_flush_global(void) {
    /* Flush entire TLB by reloading CR3 */
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    __asm__ volatile("mov %0, %%cr3" :: "r"(cr3));
}

void tlb_shootdown(uint64_t *pml4, uint64_t virt_addr) {
    /* Flush local TLB */
    __asm__ volatile("invlpg (%0)" :: "r"(virt_addr) : "memory");
    
    /* Send IPI to all other CPUs to flush their TLBs */
    for (int i = 0; i < num_cpus; i++) {
        if (i != bsp_cpu_id && cpu_data[i].state == CPU_STATE_RUNNING) {
            smp_send_ipi(i, IPI_VECTOR_TLB_FLUSH);
        }
    }
}

void tlb_shootdown_range(uint64_t *pml4, uint64_t start, uint64_t end) {
    /* Flush local TLB for the range */
    for (uint64_t addr = start; addr < end; addr += 0x1000) {
        __asm__ volatile("invlpg (%0)" :: "r"(addr) : "memory");
    }
    
    /* Broadcast TLB flush IPI */
    smp_broadcast_ipi(IPI_VECTOR_TLB_FLUSH);
}

/* ── Simple Deadlock Detection (lockdep-lite) ───────────────────── */

#define MAX_LOCK_TRACE 32

static struct {
    void *locks[MAX_LOCK_TRACE];
    const char *names[MAX_LOCK_TRACE];
    int count;
} lock_trace[MAX_CPUS];

void lockdep_init(void) {
    for (int i = 0; i < MAX_CPUS; i++) {
        lock_trace[i].count = 0;
    }
    printk(KERN_INFO "lockdep: initialized\n");
}

void lockdep_acquire(void *lock_addr, const char *lock_name) {
    int cpu = get_current_cpu_id();
    
    /* Check for potential deadlock: trying to acquire a lock we already hold */
    for (int i = 0; i < lock_trace[cpu].count; i++) {
        if (lock_trace[cpu].locks[i] == lock_addr) {
            printk(KERN_ERR "lockdep: DEADLOCK detected! CPU %d trying to reacquire '%s'\n",
                   cpu, lock_name);
            /* Continue anyway - this is just a warning in lite mode */
        }
    }
    
    /* Record this lock acquisition */
    if (lock_trace[cpu].count < MAX_LOCK_TRACE) {
        lock_trace[cpu].locks[lock_trace[cpu].count] = lock_addr;
        lock_trace[cpu].names[lock_trace[cpu].count] = lock_name;
        lock_trace[cpu].count++;
    }
}

void lockdep_release(void *lock_addr) {
    int cpu = get_current_cpu_id();
    
    /* Find and remove this lock from trace */
    for (int i = 0; i < lock_trace[cpu].count; i++) {
        if (lock_trace[cpu].locks[i] == lock_addr) {
            /* Shift remaining locks down */
            for (int j = i; j < lock_trace[cpu].count - 1; j++) {
                lock_trace[cpu].locks[j] = lock_trace[cpu].locks[j + 1];
                lock_trace[cpu].names[j] = lock_trace[cpu].names[j + 1];
            }
            lock_trace[cpu].count--;
            return;
        }
    }
}

void lockdep_check_deadlock(void) {
    /* In a full implementation, this would check lock ordering graphs */
    /* For now, we just report the current lock state */
    for (int cpu = 0; cpu < num_cpus; cpu++) {
        if (lock_trace[cpu].count > 0) {
            printk(KERN_DEBUG "lockdep: CPU %d holds %d locks:\n", cpu, lock_trace[cpu].count);
            for (int i = 0; i < lock_trace[cpu].count; i++) {
                printk(KERN_DEBUG "  [%d] %s @ %p\n", i, lock_trace[cpu].names[i], 
                       lock_trace[cpu].locks[i]);
            }
        }
    }
}
