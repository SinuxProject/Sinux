#include "sched.h"
#include "../proc/process.h"
#include "../proc/scheduler.h"
#include "../../arch/x86_64/smp/cpu.h"
#include "../../arch/x86_64/smp/lock.h"
#include "../../lib/printk.h"
#include "../../lib/string.h"

/* Global runqueues - one per CPU */
runqueue_t runqueues[MAX_CPUS];

/* Find the highest priority task in a bitmap queue */
static inline int find_first_bit(int bitmap) {
    if (bitmap == 0) return -1;
    return __builtin_ctz(bitmap);
}

/* Add a process to the appropriate priority queue */
static void enqueue_task(runqueue_t *rq, process_t *p) {
    int prio = p->priority;
    
    if (prio < 0) prio = 0;
    if (prio >= MAX_PRIORITY) prio = MAX_PRIORITY - 1;
    
    p->state = PROC_READY;
    
    /* Add to active queue */
    if (!rq->active_queue[prio]) {
        rq->active_queue[prio] = p;
        p->next = p;  /* Circular list with single element */
        rq->active_prio_bitmap |= (1 << prio);
    } else {
        process_t *head = rq->active_queue[prio];
        p->next = head->next;
        head->next = p;
    }
    
    rq->load_weight++;
}

/* Remove a process from the runqueue */
static void dequeue_task(runqueue_t *rq, process_t *p) {
    int prio = p->priority;
    
    if (prio < 0 || prio >= MAX_PRIORITY) return;
    
    process_t *head = rq->active_queue[prio];
    if (!head) return;
    
    if (head == head->next) {
        /* Only element in queue */
        rq->active_queue[prio] = NULL;
        rq->active_prio_bitmap &= ~(1 << prio);
    } else if (head == p) {
        /* Move head to next */
        rq->active_queue[prio] = head->next;
        head->next = p->next;
    } else {
        /* Find and remove from middle */
        process_t *curr = head;
        while (curr->next != head) {
            if (curr->next == p) {
                curr->next = p->next;
                break;
            }
            curr = curr->next;
        }
    }
    
    p->next = NULL;
    rq->load_weight--;
}

/* Pick the next task to run using O(1) scheduling */
process_t *sched_pick_next(runqueue_t *rq) {
    int prio;
    
    /* Check active queue first */
    prio = find_first_bit(rq->active_prio_bitmap);
    if (prio >= 0 && rq->active_queue[prio]) {
        process_t *p = rq->active_queue[prio];
        process_t *next = p->next;
        
        /* Rotate the queue at this priority level */
        if (next != p) {
            rq->active_queue[prio] = next;
        }
        
        return p;
    }
    
    /* Active queue empty - move expired to active */
    if (rq->expired_prio_bitmap != 0) {
        /* Swap active and expired queues */
        for (int i = 0; i < MAX_PRIORITY; i++) {
            rq->active_queue[i] = rq->expired_queue[i];
            rq->expired_queue[i] = NULL;
        }
        rq->active_prio_bitmap = rq->expired_prio_bitmap;
        rq->expired_prio_bitmap = 0;
        
        /* Try again */
        prio = find_first_bit(rq->active_prio_bitmap);
        if (prio >= 0 && rq->active_queue[prio]) {
            process_t *p = rq->active_queue[prio];
            rq->active_queue[prio] = p->next;
            return p;
        }
    }
    
    /* No runnable tasks - return idle */
    return rq->idle;
}

/* Context switch between two tasks */
void sched_context_switch(process_t *from, process_t *to) {
    if (from == to) return;
    
    runqueue_t *rq = &runqueues[get_current_cpu_id()];
    
    /* Save old state */
    if (from->state == PROC_RUNNING) {
        from->state = PROC_READY;
        enqueue_task(rq, from);
    }
    
    /* Set new state */
    to->state = PROC_RUNNING;
    rq->current = to;
    
    /* Update TSS.rsp0 for ring 3 -> kernel stack switching */
    extern void gdt_set_kernel_stack(uint64_t rsp);
    if (to->kstack) {
        gdt_set_kernel_stack((uint64_t)(to->kstack + KSTACK_SIZE));
    }
    
    /* Switch address space if needed */
    extern void arch_switch(uint64_t *old_rsp, uint64_t new_rsp, uint64_t cr3);
    uint64_t cr3 = to->pml4 ? (uint64_t)to->pml4 : 0;
    
    rq->sched_count++;
    
    arch_switch(&from->rsp, to->rsp, cr3);
}

/* Preemptive scheduler tick handler */
void sched_tick(void) {
    int cpu = get_current_cpu_id();
    runqueue_t *rq = &runqueues[cpu];
    
    spinlock_acquire(&rq->lock);
    
    process_t *cur = rq->current;
    if (!cur || cur == rq->idle) {
        spinlock_release(&rq->lock);
        return;
    }
    
    /* Wake up sleeping processes */
    extern uint64_t rdtsc(void);
    uint64_t now = rdtsc();
    
    /* Decrement timeslice */
    if (--cur->timeslice <= 0) {
        /* Timeslice expired - check if we should preempt */
        process_t *next = sched_pick_next(rq);
        
        if (next && next != cur && next->priority > cur->priority) {
            /* Higher priority task available - preempt */
            /* Move current to expired queue if it used full timeslice */
            dequeue_task(rq, cur);
            
            int prio = cur->priority;
            if (prio < 0) prio = 0;
            if (prio >= MAX_PRIORITY) prio = MAX_PRIORITY - 1;
            
            if (!rq->expired_queue[prio]) {
                rq->expired_queue[prio] = cur;
                cur->next = cur;
                rq->expired_prio_bitmap |= (1 << prio);
            } else {
                process_t *head = rq->expired_queue[prio];
                cur->next = head->next;
                head->next = cur;
            }
            
            /* Recalculate timeslice */
            cur->timeslice = TIMESLICE_DEFAULT + (cur->priority / 10);
            
            spinlock_release(&rq->lock);
            sched_context_switch(cur, next);
            return;
        }
        
        /* Same priority or no better task - give new timeslice */
        cur->timeslice = TIMESLICE_DEFAULT;
    }
    
    spinlock_release(&rq->lock);
}

/* Yield CPU voluntarily */
void sched_yield(void) {
    int cpu = get_current_cpu_id();
    runqueue_t *rq = &runqueues[cpu];
    
    spinlock_acquire(&rq->lock);
    
    process_t *cur = rq->current;
    if (!cur || cur == rq->idle) {
        spinlock_release(&rq->lock);
        return;
    }
    
    /* Put current task at end of its priority queue */
    dequeue_task(rq, cur);
    enqueue_task(rq, cur);
    
    /* Pick next task */
    process_t *next = sched_pick_next(rq);
    
    spinlock_release(&rq->lock);
    
    if (next && next != cur) {
        sched_context_switch(cur, next);
    }
}

/* Sleep for specified milliseconds */
void sched_sleep(uint64_t ms) {
    int cpu = get_current_cpu_id();
    runqueue_t *rq = &runqueues[cpu];
    
    spinlock_acquire(&rq->lock);
    
    process_t *cur = rq->current;
    if (!cur) {
        spinlock_release(&rq->lock);
        return;
    }
    
    /* Calculate wake-up time based on TSC */
    extern uint64_t rdtsc(void);
    extern uint64_t tsc_freq;
    
    cur->ticks = rdtsc() + (tsc_freq * ms / 1000);
    cur->state = PROC_SLEEPING;
    
    dequeue_task(rq, cur);
    
    spinlock_release(&rq->lock);
    
    sched_yield();
}

/* Wake up a sleeping process */
void sched_wake(process_t *p) {
    if (!p) return;
    
    int target_cpu = p->affinity & ((1ULL << num_cpus) - 1);
    if (target_cpu == 0) target_cpu = 0;
    
    /* Find first set bit for CPU affinity */
    for (int cpu = 0; cpu < num_cpus; cpu++) {
        if (target_cpu & (1 << cpu)) {
            runqueue_t *rq = &runqueues[cpu];
            
            spinlock_acquire(&rq->lock);
            
            if (p->state == PROC_SLEEPING || p->state == PROC_WAITING) {
                p->state = PROC_READY;
                enqueue_task(rq, p);
            }
            
            spinlock_release(&rq->lock);
            return;
        }
    }
}

/* Enqueue a process */
void sched_enqueue(process_t *p) {
    int cpu = get_current_cpu_id();
    runqueue_t *rq = &runqueues[cpu];
    
    spinlock_acquire(&rq->lock);
    enqueue_task(rq, p);
    spinlock_release(&rq->lock);
}

/* Dequeue a process */
void sched_dequeue(process_t *p) {
    int cpu = get_current_cpu_id();
    runqueue_t *rq = &runqueues[cpu];
    
    spinlock_acquire(&rq->lock);
    dequeue_task(rq, p);
    spinlock_release(&rq->lock);
}

/* Load balancing between CPUs */
void sched_balance_load(void) {
    /* Simple load balancing: if one CPU has significantly more tasks,
       steal a task from it */
    for (int from_cpu = 0; from_cpu < num_cpus; from_cpu++) {
        runqueue_t *from_rq = &runqueues[from_cpu];
        
        for (int to_cpu = 0; to_cpu < num_cpus; to_cpu++) {
            if (from_cpu == to_cpu) continue;
            
            runqueue_t *to_rq = &runqueues[to_cpu];
            
            /* If source has 2+ more tasks than destination, steal one */
            if (from_rq->load_weight >= to_rq->load_weight + 2) {
                process_t *stolen = sched_steal_task(from_cpu);
                if (stolen) {
                    spinlock_acquire(&to_rq->lock);
                    enqueue_task(to_rq, stolen);
                    spinlock_release(&to_rq->lock);
                }
            }
        }
    }
}

/* Steal a task from another CPU's runqueue */
process_t *sched_steal_task(int from_cpu) {
    if (from_cpu < 0 || from_cpu >= num_cpus) return NULL;
    
    runqueue_t *from_rq = &runqueues[from_cpu];
    
    spinlock_acquire(&from_rq->lock);
    
    /* Find lowest priority task to steal */
    for (int prio = MAX_PRIORITY - 1; prio >= 0; prio--) {
        if (from_rq->active_prio_bitmap & (1 << prio)) {
            process_t *victim = from_rq->active_queue[prio];
            if (victim && victim != from_rq->idle) {
                dequeue_task(from_rq, victim);
                spinlock_release(&from_rq->lock);
                return victim;
            }
        }
    }
    
    spinlock_release(&from_rq->lock);
    return NULL;
}

/* Set process priority */
void sched_set_priority(process_t *p, int prio) {
    if (prio < MIN_PRIORITY) prio = MIN_PRIORITY;
    if (prio >= MAX_PRIORITY) prio = MAX_PRIORITY - 1;
    
    p->priority = prio;
    p->timeslice = TIMESLICE_DEFAULT + (prio / 10);
}

/* Get process priority */
int sched_get_priority(process_t *p) {
    return p->priority;
}

/* Recalculate priority based on sleep/avg runtime */
void sched_recalculate_priority(process_t *p) {
    /* Simple heuristic: interactive processes (short sleep) get higher priority */
    /* This is a simplified version of Linux's interactive heuristic */
    int bonus = 0;
    
    if (p->sleep_avg > 0) {
        bonus = p->sleep_avg / 100;
        if (bonus > 10) bonus = 10;
    }
    
    int new_prio = DEFAULT_PRIORITY - bonus;
    if (new_prio < MIN_PRIORITY) new_prio = MIN_PRIORITY;
    if (new_prio >= MAX_PRIORITY) new_prio = MAX_PRIORITY - 1;
    
    p->priority = new_prio;
}

/* Set CPU affinity */
void sched_set_affinity(process_t *p, uint64_t cpu_mask) {
    p->affinity = cpu_mask & ((1ULL << num_cpus) - 1);
    if (p->affinity == 0) p->affinity = 1;  /* Must run on at least one CPU */
}

/* Get CPU affinity */
uint64_t sched_get_affinity(process_t *p) {
    return p->affinity;
}

/* Initialize per-CPU scheduler data */
void sched_cpu_init(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) return;
    
    runqueue_t *rq = &runqueues[cpu_id];
    
    spinlock_init(&rq->lock, "runqueue_lock");
    rq->cpu_id = cpu_id;
    rq->current = NULL;
    rq->idle = NULL;
    rq->sched_count = 0;
    rq->idle_time = 0;
    rq->load_weight = 0;
    rq->active_prio_bitmap = 0;
    rq->expired_prio_bitmap = 0;
    
    for (int i = 0; i < MAX_PRIORITY; i++) {
        rq->active_queue[i] = NULL;
        rq->expired_queue[i] = NULL;
    }
}

/* Main preemptive scheduler initialization */
void sched_preempt_init(void) {
    printk(KERN_INFO "sched: Initializing preemptive scheduler\n");
    
    lockdep_init();
    
    /* Initialize all CPU runqueues */
    for (int i = 0; i < num_cpus; i++) {
        sched_cpu_init(i);
    }
    
    printk(KERN_INFO "sched: Preemptive scheduler ready (%d CPUs)\n", num_cpus);
}
