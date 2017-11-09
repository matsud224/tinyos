#pragma once
#include <kern/types.h>

void gpe_inthandler(void);
void gpe_isr(void);

void pf_inthandler(void);
void pf_isr(u32 addr, u32 eip);

void syscall_inthandler(void);
void syscall_isr(u32 eax, u32 ebx, u32 ecx, u32 edx, u32 esi, u32 edi);
