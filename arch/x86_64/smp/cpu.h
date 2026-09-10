#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_CPUS        8
#define CPU_STATE_OFFLINE   0
#define CPU_STATE_RUNNING   1
#define CPU_STATE_HALTED    2

/* Per-CPU data structure */
typedef struct {
    int      cpu_id;          /* Logical CPU ID (APIC ID) */
    int      state;           /* CPU_STATE_* */
    uint64_t stack_top;       /* Kernel stack top for this CPU */
    uint8_t *stack_base;      /* Kernel stack base (for freeing) */
    uint64_t tsc_freq;        /* TSC frequency in Hz */
    uint64_t tsc_last;        /* Last TSC value for scheduling */
    void    *current_task;    /* Currently running task/proc */
    uint32_t lapic_id;        /* Local APIC ID */
    
    /* Per-CPU scheduler queue info */
    int      run_queue_count;
    bool     need_resched;
    
    /* Lock holder tracking (for deadlock detection) */
    void    *held_locks[16];
    int      held_lock_count;
} cpu_data_t;

/* Global CPU map and BSP data */
extern cpu_data_t cpu_data[MAX_CPUS];
extern int        num_cpus;
extern int        bsp_cpu_id;

/* Core SMP functions */
void smp_init(void);
void smp_bringup_aps(void);
void smp_cpu_halt(int cpu_id);
void smp_send_ipi(int cpu_id, int vector);
void smp_broadcast_ipi(int vector);

/* Per-CPU accessors */
static inline cpu_data_t *get_cpu_data(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) return NULL;
    return &cpu_data[cpu_id];
}

static inline int get_current_cpu_id(void) {
    uint32_t lapic_id;
    __asm__ volatile("mov %%gs:0x20, %0" : "=r"(lapic_id));
    return lapic_id;
}

static inline cpu_data_t *get_current_cpu(void) {
    return get_cpu_data(get_current_cpu_id());
}

/* TSC utilities */
static inline uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)lo | ((uint64_t)hi << 32));
}

static inline void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, 
                         uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid" 
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf));
}

/* TSC Deadline timer */
void tsc_deadline_init(void);
void tsc_deadline_set(uint64_t ticks);
void tsc_deadline_cancel(void);
