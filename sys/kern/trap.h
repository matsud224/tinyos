#pragma once
#include <kern/types.h>

void gpe_inthandler(void);
void gpe_isr(int errcode);

void pf_inthandler(void);
void pf_isr(vaddr_t addr, u32 eip, u32 esp, u32 eax);

void syscall_inthandler(void);
u32 syscall_isr(u32 eax, u32 ebx, u32 ecx, u32 edx, u32 esi, u32 edi);

void spurious_inthandler(void);
void spurious_isr(void);
