#include "cpu.h"
#include "../../lib/io.h"
#include "../../lib/printk.h"
#include "../../lib/string.h"
#include "../gdt.h"
#include "../idt.h"
#include "../pic.h"
#include "../../mm/pmm.h"
#include "../../mm/vmm.h"

/* ── LAPIC/IOAPIC Register Definitions ───────────────────────────── */
#define LAPIC_BASE_MSR       0x1B
#define LAPIC_DEFAULT_BASE   0xFEE00000ULL

/* LAPIC Registers (offsets from base) */
#define LAPIC_ID            0x020
#define LAPIC_VER           0x030
#define LAPIC_TPR           0x080
#define LAPIC_APR           0x090
#define LAPIC_PPR           0x0A0
#define LAPIC_EOI           0x0B0
#define LAPIC_LDR           0x0D0
#define LAPIC_DFR           0x0E0
#define LAPIC_SVRR          0x0F0
#define LAPIC_ISR           0x100
#define LAPIC_TMR           0x180
#define LAPIC_IRR           0x200
#define LAPIC_ESR           0x280
#define LAPIC_ICR_LO        0x300
#define LAPIC_ICR_HI        0x310
#define LAPIC_LVT_TIMER     0x320
#define LAPIC_LVT_THERMAL   0x330
#define LAPIC_LVT_PERF      0x340
#define LAPIC_LVT_LINT0     0x350
#define LAPIC_LVT_LINT1     0x360
#define LAPIC_LVT_ERROR     0x370
#define LAPIC_INIT_COUNT    0x380
#define LAPIC_CUR_COUNT     0x390
#define LAPIC_DIV_CONF      0x3E0

/* LAPIC Timer Modes */
#define LAPIC_TIMER_ONE_SHOT    0x00000
#define LAPIC_TIMER_PERIODIC    0x20000
#define LAPIC_TIMER_TSC_DEADLINE 0x40000

/* LAPIC Delivery Modes */
#define LAPIC_DELIV_FIXED       0x000
#define LAPIC_DELIV_LOWEST      0x100
#define LAPIC_DELIV_SMI         0x200
#define LAPIC_DELIV_NMI         0x400
#define LAPIC_DELIV_INIT        0x500
#define LAPIC_DELIV_STARTUP     0x600
#define LAPIC_DELIV_EXTINT      0x700

/* LAPIC Destination Modes */
#define LAPIC_DEST_PHYSICAL     0x0000
#define LAPIC_DEST_LOGICAL      0x0800

/* LAPIC Trigger Modes */
#define LAPIC_LEVEL_DEASSERT    0x00000
#define LAPIC_LEVEL_ASSERT      0x04000
#define LAPIC_TRIGGER_EDGE      0x00000
#define LAPIC_TRIGGER_LEVEL     0x08000

/* IPI Vectors */
#define IPI_VECTOR_INIT         0x00
#define IPI_VECTOR_STARTUP      0x00
#define IPI_VECTOR_RESCHED      0xFA
#define IPI_VECTOR_TLB_FLUSH    0xFB
#define IPI_VECTOR_SPURIOUS     0xFF

/* IOAPIC Registers */
#define IOAPIC_REG_SELECT       0x00
#define IOAPIC_REG_WINDOW       0x10
#define IOAPIC_REG_ID           0x00
#define IOAPIC_REG_VER          0x01
#define IOAPIC_REG_ARB          0x02
#define IOAPIC_REDIR_TBL        0x10

/* IOAPIC Redirection Entry Fields */
#define IOAPIC_MASKED           (1 << 16)
#define IOAPIC_TRIGGER_EDGE     (0 << 15)
#define IOAPIC_TRIGGER_LEVEL    (1 << 15)
#define IOAPIC_POLARITY_HIGH    (0 << 13)
#define IOAPIC_POLARITY_LOW     (1 << 13)
#define IOAPIC_DEST_PHYSICAL    (0 << 11)
#define IOAPIC_DEST_LOGICAL     (1 << 11)
#define IOAPIC_DELIV_FIXED      (0 << 8)
#define IOAPIC_DELIV_LOWEST     (1 << 8)
#define IOAPIC_DELIV_SMI        (2 << 8)
#define IOAPIC_DELIV_NMI        (4 << 8)
#define IOAPIC_DELIV_EXTINT     (7 << 8)

/* Global state */
static uint64_t lapic_base = 0;
static uint64_t ioapic_base = 0;
static bool lapic_enabled = false;

cpu_data_t cpu_data[MAX_CPUS];
int num_cpus = 1;
int bsp_cpu_id = 0;

/* ── LAPIC Access Functions ─────────────────────────────────────── */
static inline void lapic_write(uint32_t reg, uint32_t val) {
    if (!lapic_enabled || !lapic_base) return;
    *((volatile uint32_t *)(lapic_base + reg)) = val;
    /* Read-back to ensure write completion */
    (void)*((volatile uint32_t *)(lapic_base + LAPIC_ID));
}

static inline uint32_t lapic_read(uint32_t reg) {
    if (!lapic_enabled || !lapic_base) return 0;
    return *((volatile uint32_t *)(lapic_base + reg));
}

/* ── IOAPIC Access Functions ────────────────────────────────────── */
static inline void ioapic_write(uint32_t reg, uint32_t val) {
    if (!ioapic_base) return;
    *((volatile uint32_t *)(ioapic_base + IOAPIC_REG_SELECT)) = reg;
    *((volatile uint32_t *)(ioapic_base + IOAPIC_REG_WINDOW)) = val;
}

static inline uint32_t ioapic_read(uint32_t reg) {
    if (!ioapic_base) return 0;
    *((volatile uint32_t *)(ioapic_base + IOAPIC_REG_SELECT)) = reg;
    return *((volatile uint32_t *)(ioapic_base + IOAPIC_REG_WINDOW));
}

/* ── LAPIC Initialization ───────────────────────────────────────── */
void lapic_init(void) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Check for LAPIC support via CPUID */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if (!(edx & (1 << 9))) {
        printk(KERN_WARNING "smp: LAPIC not supported\n");
        return;
    }
    
    /* Get LAPIC base address from MSR */
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(LAPIC_BASE_MSR));
    if (!(lo & (1 << 11))) {
        printk(KERN_WARNING "smp: LAPIC disabled in MSR\n");
        return;
    }
    
    lapic_base = (lo & 0xFFFFFF000ULL);
    lapic_enabled = true;
    
    /* Enable LAPIC */
    uint32_t svrr = lapic_read(LAPIC_SVRR);
    svrr |= 0x1FF;  /* Enable LAPIC, set spurious vector */
    lapic_write(LAPIC_SVRR, svrr);
    
    /* Set LDR to flat mode for logical destination */
    lapic_write(LAPIC_DFR, 0xFFFFFFFF);
    lapic_write(LAPIC_LDR, 0x00000000);
    
    /* Mask all LVT entries initially */
    lapic_write(LAPIC_LVT_TIMER,    LAPIC_TIMER_PERIODIC | 0x10000);
    lapic_write(LAPIC_LVT_THERMAL,  0x10000);
    lapic_write(LAPIC_LVT_PERF,     0x10000);
    lapic_write(LAPIC_LVT_LINT0,    0x10000);
    lapic_write(LAPIC_LVT_LINT1,    0x10000);
    lapic_write(LAPIC_LVT_ERROR,    0x10000);
    
    /* Set Task Priority Register to accept all interrupts */
    lapic_write(LAPIC_TPR, 0);
    
    printk(KERN_INFO "smp: LAPIC enabled at 0x%lx\n", lapic_base);
}

/* ── IOAPIC Initialization ──────────────────────────────────────── */
void ioapic_init(void) {
    /* IOAPIC is typically at 0xFEC00000 on QEMU */
    ioapic_base = 0xFEC00000ULL;
    
    uint32_t ver = ioapic_read(IOAPIC_REG_VER);
    int max_redir = (ver >> 16) & 0xFF;
    
    printk(KERN_INFO "smp: IOAPIC enabled at 0x%lx, %d redirection entries\n", 
           ioapic_base, max_redir + 1);
    
    /* Mask all IRQs initially */
    for (int i = 0; i <= max_redir; i++) {
        ioapic_write(IOAPIC_REDIR_TBL + i * 2, IOAPIC_MASKED);
        ioapic_write(IOAPIC_REDIR_TBL + i * 2 + 1, 0);
    }
}

/* ── LAPIC Timer with TSC Deadline ──────────────────────────────── */
void tsc_deadline_init(void) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Check for TSC Deadline support */
    cpuid(1, &eax, &ebx, &ecx, &edx);
    if (!(ecx & (1 << 24))) {
        printk(KERN_INFO "smp: TSC Deadline not supported, using periodic timer\n");
        return;
    }
    
    /* Switch to TSC Deadline mode */
    uint32_t timer_reg = lapic_read(LAPIC_LVT_TIMER);
    timer_reg &= ~0x20000;  /* Clear periodic bit */
    timer_reg |= 0x40000;   /* Set TSC deadline bit */
    lapic_write(LAPIC_LVT_TIMER, timer_reg);
    
    printk(KERN_INFO "smp: TSC Deadline timer enabled\n");
}

void tsc_deadline_set(uint64_t ticks) {
    if (!lapic_enabled) return;
    
    /* Write deadline to IA32_TSC_DEADLINE MSR (0x6E0) */
    uint32_t lo = (uint32_t)(ticks & 0xFFFFFFFF);
    uint32_t hi = (uint32_t)(ticks >> 32);
    __asm__ volatile("wrmsr" :: "c"(0x6E0), "a"(lo), "d"(hi));
}

void tsc_deadline_cancel(void) {
    if (!lapic_enabled) return;
    __asm__ volatile("wrmsr" :: "c"(0x6E0), "a"(0), "d"(0));
}

/* ── LAPIC Timer EOI ────────────────────────────────────────────── */
void lapic_eoi(void) {
    if (!lapic_enabled) return;
    lapic_write(LAPIC_EOI, 0);
}

/* ── IPI (Inter-Processor Interrupt) Functions ──────────────────── */
void smp_send_ipi(int cpu_id, int vector) {
    if (!lapic_enabled) return;
    
    /* Wait for previous IPI to complete */
    while (lapic_read(LAPIC_ICR_LO) & (1 << 12));
    
    /* Set destination APIC ID */
    lapic_write(LAPIC_ICR_HI, cpu_id << 24);
    
    /* Send IPI */
    uint32_t icr_lo = vector | LAPIC_DELIV_FIXED | LAPIC_DEST_PHYSICAL | 
                      LAPIC_TRIGGER_EDGE | LAPIC_LEVEL_ASSERT;
    lapic_write(LAPIC_ICR_LO, icr_lo);
}

void smp_broadcast_ipi(int vector) {
    if (!lapic_enabled) return;
    
    while (lapic_read(LAPIC_ICR_LO) & (1 << 12));
    
    /* Broadcast to all CPUs including self */
    uint32_t icr_lo = vector | LAPIC_DELIV_FIXED | LAPIC_DEST_LOGICAL |
                      LAPIC_TRIGGER_EDGE | LAPIC_LEVEL_ASSERT;
    lapic_write(LAPIC_ICR_LO, icr_lo);
}

/* ── CPU Halt Function ──────────────────────────────────────────── */
void smp_cpu_halt(int cpu_id) {
    if (cpu_id < 0 || cpu_id >= MAX_CPUS) return;
    cpu_data[cpu_id].state = CPU_STATE_HALTED;
    
    /* Send INIT IPI to halt the CPU */
    while (lapic_read(LAPIC_ICR_LO) & (1 << 12));
    lapic_write(LAPIC_ICR_HI, cpu_data[cpu_id].lapic_id << 24);
    lapic_write(LAPIC_ICR_LO, LAPIC_DELIV_INIT | LAPIC_DEST_PHYSICAL |
                LAPIC_TRIGGER_EDGE | LAPIC_LEVEL_ASSERT);
}

/* ── AP Startup Code (Trampoline) ───────────────────────────────── */
extern uint8_t _trampoline_start[], _trampoline_end[];
extern uint64_t _trampoline_pml4;
extern uint64_t _trampoline_stack_top;
extern uint64_t _trampoline_long_mode_entry;

void ap_entry(void);

/* Trampoline code must be in low memory (below 1MB) for SIPI */
__attribute__((section(".trampoline")))
void trampoline_start(void) {
    /* This code runs in real mode, then protected mode, then long mode */
    __asm__ volatile(
        ".code16\n\t"
        "cli\n\t"
        "xor %ax, %ax\n\t"
        "mov %ax, %ds\n\t"
        "mov %ax, %es\n\t"
        "mov %ax, %ss\n\t"
        "mov $0x8000, %esp\n\t"
        
        /* Enable A20 */
        "in $0x92, %al\n\t"
        "or $0x02, %al\n\t"
        "out %al, $0x92\n\t"
        
        /* Load GDT and enter protected mode */
        "lgdt trampoline_gdt_desc\n\t"
        "mov %cr0, %eax\n\t"
        "or $1, %eax\n\t"
        "mov %eax, %cr0\n\t"
        "ljmp $0x08, $trampoline_prot_mode\n\t"
        
        ".code32\n\t"
        "trampoline_prot_mode:\n\t"
        "mov $0x10, %ax\n\t"
        "mov %ax, %ds\n\t"
        "mov %ax, %es\n\t"
        "mov %ax, %fs\n\t"
        "mov %ax, %gs\n\t"
        "mov %ax, %ss\n\t"
        "mov $_trampoline_stack_top, %esp\n\t"
        
        /* Enable PAE and PGE */
        "mov %cr4, %eax\n\t"
        "or $(1<<5)|(1<<4), %eax\n\t"
        "mov %eax, %cr4\n\t"
        
        /* Load CR3 with PML4 */
        "mov _trampoline_pml4, %eax\n\t"
        "mov %eax, %cr3\n\t"
        
        /* Enable Long Mode */
        "mov $0xC0000080, %ecx\n\t"
        "rdmsr\n\t"
        "or $(1<<8), %eax\n\t"
        "wrmsr\n\t"
        
        /* Enable Paging */
        "mov %cr0, %eax\n\t"
        "or $(1<<31)|(1<<0), %eax\n\t"
        "mov %eax, %cr0\n\t"
        
        /* Jump to long mode */
        "ljmp $0x08, $_trampoline_long_mode_entry\n\t"
        
        ".align 16\n\t"
        "trampoline_gdt:\n\t"
        ".quad 0\n\t"
        ".quad 0x00AF9A000000FFFF  /* Code segment */\n\t"
        ".quad 0x00CF92000000FFFF  /* Data segment */\n\t"
        "trampoline_gdt_desc:\n\t"
        ".word trampoline_gdt_desc - trampoline_gdt - 1\n\t"
        ".quad trampoline_gdt\n\t"
    );
}

/* ── AP Bring-up ────────────────────────────────────────────────── */
void smp_bringup_aps(void) {
    if (!lapic_enabled) {
        printk(KERN_WARNING "smp: Cannot bring up APs without LAPIC\n");
        return;
    }
    
    /* Copy trampoline code to low memory (0x7000) */
    extern uint8_t _trampoline_start[], _trampoline_end[];
    size_t trampoline_size = _trampoline_end - _trampoline_start;
    
    if (trampoline_size > 0x1000) {
        printk(KERN_ERR "smp: Trampoline too large (%zu bytes)\n", trampoline_size);
        return;
    }
    
    /* Map trampoline region identity */
    vmm_map_range(vmm_kernel_pml4(), 0x7000, 0x7000, 0x1000, 
                  VMM_PRESENT | VMM_WRITABLE);
    
    kmemcpy((void *)0x7000, _trampoline_start, trampoline_size);
    
    /* Count CPUs via ACPI/MADT (simplified: assume 4 CPUs for QEMU) */
    int target_cpus = 4;
    
    for (int apic_id = 1; apic_id < target_cpus && num_cpus < MAX_CPUS; apic_id++) {
        /* Initialize per-CPU data */
        int cpu_id = num_cpus;
        cpu_data[cpu_id].cpu_id = cpu_id;
        cpu_data[cpu_id].lapic_id = apic_id;
        cpu_data[cpu_id].state = CPU_STATE_OFFLINE;
        
        /* Allocate kernel stack for AP */
        cpu_data[cpu_id].stack_base = kmalloc(16384);
        cpu_data[cpu_id].stack_top = (uint64_t)(cpu_data[cpu_id].stack_base + 16384);
        
        /* Set trampoline parameters */
        _trampoline_pml4 = (uint64_t)vmm_kernel_pml4();
        _trampoline_stack_top = cpu_data[cpu_id].stack_top;
        _trampoline_long_mode_entry = (uint64_t)ap_entry;
        
        printk(KERN_INFO "smp: Sending INIT to APIC ID %d\n", apic_id);
        
        /* Send INIT IPI */
        while (lapic_read(LAPIC_ICR_LO) & (1 << 12));
        lapic_write(LAPIC_ICR_HI, apic_id << 24);
        lapic_write(LAPIC_ICR_LO, LAPIC_DELIV_INIT | LAPIC_DEST_PHYSICAL |
                    LAPIC_TRIGGER_EDGE | LAPIC_LEVEL_ASSERT);
        
        /* Wait 10ms */
        for (volatile int i = 0; i < 100000; i++);
        
        /* Send STARTUP IPI with vector 0x07 (entry at 0x7000) */
        while (lapic_read(LAPIC_ICR_LO) & (1 << 12));
        lapic_write(LAPIC_ICR_HI, apic_id << 24);
        lapic_write(LAPIC_ICR_LO, 0x07 | LAPIC_DELIV_STARTUP | LAPIC_DEST_PHYSICAL);
        
        /* Wait for AP to start */
        for (volatile int i = 0; i < 1000000; i++);
        
        if (cpu_data[cpu_id].state == CPU_STATE_RUNNING) {
            num_cpus++;
            printk(KERN_INFO "smp: AP %d started successfully\n", cpu_id);
        } else {
            printk(KERN_WARNING "smp: AP %d failed to start\n", cpu_id);
        }
    }
    
    printk(KERN_INFO "smp: Total CPUs online: %d\n", num_cpus);
}

/* ── AP Entry Point (called after AP boots) ─────────────────────── */
void ap_entry(void) {
    int cpu_id = get_current_cpu_id();
    
    /* Initialize per-CPU structures */
    gdt_init();
    idt_init();
    
    /* Enable interrupts on this AP */
    lapic_write(LAPIC_TPR, 0);
    lapic_write(LAPIC_SVRR, lapic_read(LAPIC_SVRR) | 0x100);
    
    /* Mark CPU as running */
    cpu_data[cpu_id].state = CPU_STATE_RUNNING;
    cpu_data[cpu_id].tsc_last = rdtsc();
    
    printk(KERN_INFO "smp: CPU %d (APIC ID %d) online\n", 
           cpu_id, cpu_data[cpu_id].lapic_id);
    
    /* Enable interrupts */
    __asm__ volatile("sti");
    
    /* Enter idle loop */
    for (;;) {
        __asm__ volatile("hlt");
    }
}

/* ── Main SMP Initialization ────────────────────────────────────── */

/* Trampoline variables - defined in linker script */
uint64_t _trampoline_pml4 = 0;
uint64_t _trampoline_stack_top = 0;
uint64_t _trampoline_long_mode_entry = 0;

/* TSC frequency (calibrated during boot) */
static uint64_t tsc_freq = 0;

/* Calibrate TSC using CPUID */
static void calibrate_tsc(void) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Get TSC frequency via CPUID leaf 0x15 (Intel Skylake+) */
    cpuid(0x15, &eax, &ebx, &ecx, &edx);
    if (eax && ebx && ecx) {
        /* TSC frequency = crystal clock * EBX / EAX */
        tsc_freq = ((uint64_t)ecx * ebx) / eax;
        printk(KERN_INFO "smp: TSC frequency: %lu Hz (via CPUID)\n", tsc_freq);
        return;
    }
    
    /* Fallback: assume 2.5 GHz for QEMU */
    tsc_freq = 2500000000ULL;
    printk(KERN_INFO "smp: TSC frequency: %lu Hz (assumed)\n", tsc_freq);
}

void smp_init(void) {
    printk(KERN_INFO "smp: Initializing SMP subsystem\n");
    
    /* Initialize BSP CPU data */
    cpu_data[0].cpu_id = 0;
    cpu_data[0].state = CPU_STATE_RUNNING;
    cpu_data[0].lapic_id = lapic_read(LAPIC_ID) >> 24;
    cpu_data[0].tsc_last = rdtsc();
    bsp_cpu_id = 0;
    
    /* Calibrate TSC frequency */
    calibrate_tsc();
    cpu_data[0].tsc_freq = tsc_freq;
    
    /* Initialize LAPIC and IOAPIC */
    lapic_init();
    ioapic_init();
    
    /* Initialize TSC Deadline timer */
    tsc_deadline_init();
}
