#pragma once

#include <stdint.h>

void gpe_inthandler(void);
void gpe_isr(void);

void pf_inthandler(void);
void pf_isr(uint32_t errcode, uint32_t addr);

void syscall_inthandler(void);
void syscall_isr(void);
