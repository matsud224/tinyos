#pragma once

#include <stdint.h>

void gpe_inthandler(void);
void gpe_isr(void);

void pf_inthandler(void);
void pf_isr(uint32_t addr);

void syscall_inthandler(void);
void syscall_isr(uint32_t eax, uint32_t ebx, uint32_t ecx, uint32_t edx, uint32_t esi, uint32_t edi);
