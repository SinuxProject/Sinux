#pragma once
#include "../../arch/x86_64/smp/lock.h"
#include "../proc/process.h"
#include "../../arch/x86_64/smp/cpu.h"
#include <stdint.h>
#include <stdbool.h>

#define MAX_PRIORITY    140
#define MIN_PRIORITY    0
#define DEFAULT_PRIORITY 120
#define NICE_TO_PRIO(nice) (DEFAULT_PRIORITY + (nice))
#define PRIO_TO_NICE(prio) ((prio) - DEFAULT_PRIORITY)

#define TIMESLICE_MIN   1
#define TIMESLICE_MAX   200
#define TIMESLICE_DEFAULT 5

/* Scheduler states */
#define SCHED_STATE_RUNNING     0
#define SCHED_STATE_READY       1
#define SCHED_STATE_BLOCKED     2

/* Per-CPU runqueue */
typedef struct runqueue {
    spinlock_t lock;
    
    /* Priority arrays for O(1) scheduling */
    process_t *active_queue[MAX_PRIORITY];
    process_t *expired_queue[MAX_PRIORITY];
    int active_prio_bitmap;
    int expired_prio_bitmap;
    
    /* Current running task */
    process_t *current;
    process_t *idle;
    
    /* Statistics */
    uint64_t sched_count;
    uint64_t idle_time;
    
    /* Load balancing */
    uint64_t load_weight;
    int cpu_id;
} runqueue_t;

extern runqueue_t runqueues[MAX_CPUS];

/* Scheduler operations */
void sched_preempt_init(void);
void sched_cpu_init(int cpu_id);
void sched_enqueue(process_t *p);
void sched_dequeue(process_t *p);
void sched_tick(void);
void sched_yield(void);
void sched_sleep(uint64_t ms);
void sched_wake(process_t *p);
process_t *sched_pick_next(runqueue_t *rq);
void sched_context_switch(process_t *from, process_t *to);

/* Load balancing */
void sched_balance_load(void);
process_t *sched_steal_task(int from_cpu);

/* Priority management */
void sched_set_priority(process_t *p, int prio);
int sched_get_priority(process_t *p);
void sched_recalculate_priority(process_t *p);

/* CPU affinity */
void sched_set_affinity(process_t *p, uint64_t cpu_mask);
uint64_t sched_get_affinity(process_t *p);
