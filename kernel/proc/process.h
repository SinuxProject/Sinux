#pragma once
#include <stdint.h>
#include <stddef.h>
#include "../ipc/signal.h"

#define MAX_PROCS   256
#define MAX_FDS     64
#define KSTACK_SIZE 8192

typedef enum {
    PROC_UNUSED  = 0,
    PROC_RUNNING,
    PROC_READY,
    PROC_SLEEPING,
    PROC_ZOMBIE,
    PROC_WAITING,
} proc_state_t;

typedef int pid_t;

struct file;
typedef struct {
    struct file *file;
    int          flags;
} fd_entry_t;

typedef struct process {
    pid_t        pid;
    pid_t        ppid;
    proc_state_t state;
    int          exit_code;

    uint64_t     rsp;
    uint64_t    *pml4;
    uint8_t     *kstack;

    uint64_t     ticks;
    int          timeslice;
    
    /* SMP scheduler fields */
    int          priority;       /* 0 (highest) to 139 (lowest) */
    int          nice;           /* -20 to +19 */
    uint64_t     sleep_avg;      /* For interactive task detection */
    uint64_t     affinity;       /* CPU affinity mask */
    int          cpu;            /* Last CPU this ran on */

    fd_entry_t   fds[MAX_FDS];
    char         cwd[256];

    uint64_t     sig_pending;
    uint64_t     sig_blocked;
    sigaction_t  sig_actions[NSIG];

    uint64_t     brk;
    uint64_t     brk_start;

    struct process *next;
} process_t;

void       proc_init(void);
process_t *proc_create(void);
void       proc_exit(process_t *p, int code);
process_t *proc_find(pid_t pid);
process_t *proc_current(void);
void       proc_set_current(process_t *p);
int        proc_alloc_fd(process_t *p, struct file *f);
void       proc_free_fd(process_t *p, int fd);
