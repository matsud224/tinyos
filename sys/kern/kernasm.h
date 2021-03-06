#pragma once
#include <kern/types.h>

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
u32 geteflags(void);
void flushtlb(void *addr);
void a20_enable(void);
void saveesp(void);
void _thread_yield(void);
void cpu_halt(void);
u32 xchg(u32 value, void *mem);
void jmpto_current(void);
void jmpto_userspace(void *entrypoint, void *userstack);
u32 getesp(void);
u32 fork_prologue(u32 (*func)(u32, u32, u32, u32, u32, u32));
u32 fork_child_epilogue(void);

