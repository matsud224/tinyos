#pragma once

#include <stdint.h>

void gpe_inthandler(void);
void gpe_isr(void);

void pf_inthandler(uint32_t errcode, uint32_t addr);
void pf_isr(void);
