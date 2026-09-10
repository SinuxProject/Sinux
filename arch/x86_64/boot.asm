
section .multiboot2
bits 32

MB2_MAGIC    equ 0xE85250D6
MB2_ARCH     equ 0
MB2_LEN      equ (mb2_end - mb2_start)
MB2_CHECKSUM equ (0x100000000 - (MB2_MAGIC + MB2_ARCH + MB2_LEN))

align 8
mb2_start:
    dd MB2_MAGIC
    dd MB2_ARCH
    dd MB2_LEN
    dd MB2_CHECKSUM

    ; ── End tag ───────────────────────────────────────────
    dw 0
    dw 0
    dd 8
mb2_end:

section .bss
align 4096
pml4: resb 4096
pdpt: resb 4096
pdt:  resb 4096
align 16
stack_bottom: resb 32768
stack_top:

section .data
mb2_magic_save: dd 0
mb2_info_save:  dd 0

section .rodata
align 8
gdt64:
    dq 0
.code: equ $ - gdt64
    dq (1<<43)|(1<<44)|(1<<47)|(1<<53)
.data: equ $ - gdt64
    dq (1<<41)|(1<<44)|(1<<47)
gdt64_ptr:
    dw $ - gdt64 - 1
    dq gdt64

section .text
bits 32
global _start

_start:
    mov  esp, stack_top
    mov  [mb2_magic_save], eax
    mov  [mb2_info_save],  ebx

    cmp  eax, 0x36D76289
    jne  .bad_magic

    call check_cpuid
    call check_lm
    call build_pages
    call enable_paging
    lgdt [gdt64_ptr]
    jmp  gdt64.code : long_mode_entry

.bad_magic:
    mov  dl, '0'
    jmp  panic

panic:
    mov  dword [0xB8000], 0x4F724F45
    mov  dword [0xB8004], 0x4F3A4F72
    mov  byte  [0xB8008], dl
    mov  byte  [0xB8009], 0x4F
.halt: cli
       hlt
       jmp .halt

check_cpuid:
    pushfd
    pop  eax
    mov  ecx, eax
    xor  eax, (1<<21)
    push eax
    popfd
    pushfd
    pop  eax
    push ecx
    popfd
    cmp  eax, ecx
    jne  .ok
    mov  dl, '1'
    jmp  panic
.ok: ret

check_lm:
    mov  eax, 0x80000000
    cpuid
    cmp  eax, 0x80000001
    jb   .fail
    mov  eax, 0x80000001
    cpuid
    test edx, (1<<29)
    jnz  .ok
.fail:
    mov  dl, '2'
    jmp  panic
.ok: ret

build_pages:
    mov  eax, pdpt
    or   eax, 0x3
    mov  [pml4], eax
    mov  eax, pdt
    or   eax, 0x3
    mov  [pdpt], eax
    xor  ecx, ecx
.loop:
    mov  eax, 0x200000
    mul  ecx
    or   eax, 0x83
    mov  [pdt + ecx*8], eax
    inc  ecx
    cmp  ecx, 512
    jne  .loop
    ret

enable_paging:
    mov  eax, pml4
    mov  cr3, eax
    mov  eax, cr4
    or   eax, (1<<5)
    mov  cr4, eax
    mov  ecx, 0xC0000080
    rdmsr
    or   eax, (1<<8)
    wrmsr
    mov  eax, cr0
    or   eax, (1<<31)|(1<<0)
    mov  cr0, eax
    ret

bits 64
DEFAULT REL

extern kernel_main
extern handle_exception
extern handle_keyboard
extern handle_pit
extern syscall_entry

long_mode_entry:
    mov  ax, gdt64.data
    mov  ds, ax
    mov  es, ax
    mov  fs, ax
    mov  gs, ax
    mov  ss, ax
    mov  edi, [mb2_magic_save]
    mov  esi, [mb2_info_save]
    call kernel_main
.halt:
    cli
    hlt
    jmp .halt

global arch_switch
arch_switch:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    pushfq
    mov  [rdi], rsp
    mov  rsp,  rsi
    test rdx, rdx
    jz   .no_cr3
    mov  cr3, rdx
.no_cr3:
    popfq
    pop  r15
    pop  r14
    pop  r13
    pop  r12
    pop  rbp
    pop  rbx
    ret

%macro PUSH_ALL 0
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
%endmacro

%macro ISR_NOERR 2
global %2
%2:
    cli
    push qword 0
    push qword %1
    jmp  exc_common
%endmacro

%macro ISR_ERR 2
global %2
%2:
    cli
    push qword %1
    jmp  exc_common
%endmacro

ISR_NOERR  0,  isr_de
ISR_NOERR  6,  isr_ud
ISR_NOERR  8,  isr_df
ISR_ERR   13,  isr_gp
ISR_ERR   14,  isr_pf

exc_common:
    PUSH_ALL
    mov  rdi, [rsp + 15*8]
    mov  rsi, [rsp + 16*8]
    call handle_exception
    POP_ALL
    add  rsp, 16
    iretq

global isr_kbd
isr_kbd:
    PUSH_ALL
    call handle_keyboard
    POP_ALL
    iretq

global isr_pit
isr_pit:
    PUSH_ALL
    call handle_pit
    POP_ALL
    iretq

; ── IPI handlers for SMP ────────────────────────────────────────────
global isr_resched
isr_resched:
    PUSH_ALL
    mov rdi, 0xFEE000B0
    mov dword [rdi], 0
    POP_ALL
    iretq

global isr_tlb_flush
isr_tlb_flush:
    PUSH_ALL
    mov rax, cr3
    mov cr3, rax
    mov rdi, 0xFEE000B0
    mov dword [rdi], 0
    POP_ALL
    iretq

; ── User context save area (for fork) ──────────────────────────────
section .bss
global user_ctx_rip, user_ctx_rflags, user_ctx_rsp
user_ctx_rip:    resq 1
user_ctx_rflags: resq 1
user_ctx_rsp:    resq 1
section .text

global syscall_asm_entry
syscall_asm_entry:

    ; Capture user state BEFORE any stack changes — fork() reads these
    mov [user_ctx_rip],    rcx   ; user RIP  (SYSCALL saves RIP → RCX)
    mov [user_ctx_rflags], r11   ; user RFLAGS (SYSCALL saves RFLAGS → R11)
    mov [user_ctx_rsp],    rsp   ; user RSP (unchanged at SYSCALL entry)

    push rcx
    push r11
    push rbp

    mov  rcx, r10

    call syscall_entry

    pop  rbp
    pop  r11
    pop  rcx
    o64 sysret

; ── fork child trampoline ───────────────────────────────────────────
; Called by arch_switch when the forked child is first scheduled.
; arch_switch restores: r15, r14, r13, r12, rbp, rbx
; We encode:  r12 = user RIP
;             r13 = user RSP
;             r14 = user RFLAGS
global fork_child_stub
fork_child_stub:
    mov ax, 0x23        ; restore user data segments
    mov ds, ax
    mov es, ax
    xor rax, rax        ; child returns 0 from fork()
    mov rcx, r12        ; user RIP  → sysretq reads rcx
    mov r11, r14        ; user RFLAGS → sysretq reads r11
    mov rsp, r13        ; switch back to user stack (pre-syscall RSP)
    o64 sysret

global _enter_usermode
_enter_usermode:
    cli

    mov ax, 0x23
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push qword 0x23        ; SS  (user data selector | RPL3)
    push rsi               ; RSP (user stack)
    push qword 0x202       ; RFLAGS (IF=1, reserved bit 1)
    push qword 0x1B        ; CS  (user code selector | RPL3)
    push rdi               ; RIP (entry point)
    iretq
