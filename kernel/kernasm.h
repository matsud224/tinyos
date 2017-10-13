#pragma once
#include "types.h"

void divzero(void);
void gengpe(void);
void out8(u16 port, u8 val);
void out16(u16 port, u16 val);
void out32(u16 port, u32 val);
u8 in8(u16 port);
u16 in16(u16 port);
u32 in32(u16 port);
void lidt(void *p);
void lgdt(void *p);
void ltr(u16 sel);
void sti(void);
void cli(void);
u32 getcr2(void);
void flushtlb(void *addr);
void a20_enable(void);
void saveesp(void);
void task_yield(void);
void cpu_halt(void);
u32 xchg(u32 value, void *mem);
void jmpto_current(void);
