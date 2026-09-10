#include "idt.h"
#include "../../lib/string.h"
#include <stdint.h>

typedef struct __attribute__((packed)) {
    uint16_t off0, sel;
    uint8_t  ist, attr;
    uint16_t off16;
    uint32_t off32, _zero;
} idt_gate_t;

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint64_t base;
} idtr_t;

static idt_gate_t idt[256];
static idtr_t     idtr;

/* External ISR declarations */
extern void isr_de(void), isr_ud(void), isr_df(void);
extern void isr_gp(void), isr_pf(void);
extern void isr_kbd(void), isr_pit(void);

/* IPI handlers for SMP */
extern void isr_resched(void);
extern void isr_tlb_flush(void);

void
idt_set_gate(int n, uint64_t h, uint8_t attr)
{
    idt[n].off0  = (uint16_t)(h & 0xFFFF);
    idt[n].sel   = 0x08;
    idt[n].ist   = 0;
    idt[n].attr  = attr;
    idt[n].off16 = (uint16_t)((h >> 16) & 0xFFFF);
    idt[n].off32 = (uint32_t)(h >> 32);
    idt[n]._zero = 0;
}

void
idt_init(void)
{
    kmemset(idt, 0, sizeof(idt));
    idt_set_gate( 0, (uint64_t)isr_de,  IDT_INT_GATE);
    idt_set_gate( 6, (uint64_t)isr_ud,  IDT_INT_GATE);
    idt_set_gate( 8, (uint64_t)isr_df,  IDT_INT_GATE);
    idt_set_gate(13, (uint64_t)isr_gp,  IDT_INT_GATE);
    idt_set_gate(14, (uint64_t)isr_pf,  IDT_INT_GATE);
    idt_set_gate(32, (uint64_t)isr_pit, IDT_INT_GATE);
    idt_set_gate(33, (uint64_t)isr_kbd, IDT_INT_GATE);
    
    /* Register IPI handlers for SMP */
    idt_set_gate(0xFA, (uint64_t)isr_resched, IDT_INT_GATE);   /* Reschedule IPI */
    idt_set_gate(0xFB, (uint64_t)isr_tlb_flush, IDT_INT_GATE); /* TLB flush IPI */

    idtr.limit = (uint16_t)(sizeof(idt) - 1);
    idtr.base  = (uint64_t)idt;
    __asm__ volatile ("lidt %0" :: "m"(idtr) : "memory");
}
