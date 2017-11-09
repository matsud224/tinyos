#pragma once

#include <kern/vmem.h>
#include <kern/list.h>
#include <stdint.h>
#include <stddef.h>

extern struct task *current;

struct tss {
  u16 backlink;	u16 f1;
  u32 esp0;
  u16 ss0;				u16 f2;
  u32 esp1;
  u16 ss1;				u16 f3;
  u32 esp2;
  u16 ss2;				u16 f4;
  u32 cr3;
  u32 eip;
  u32 eflags;
  u32 eax;
  u32 ecx;
  u32 edx;
  u32 ebx;
  u32 esp;
  u32 ebp;
  u32 esi;
  u32 edi;
  u16 es;				u16 f5;
  u16 cs;				u16 f6;
  u16 ss;				u16 f7;
  u16 ds;				u16 f8;
  u16 fs;				u16 f9;
  u16 gs;				u16 f10;
  u16 ldt;				u16 f11;
  u16 t;					u16 iobase;
};

struct task_state {
  u32 esp;
  u32 cr3;
};

#define TASK_STATE_RUNNING	0
#define TASK_STATE_WAITING	1
#define TASK_STATE_EXITED		2

struct task {
  struct task_state regs;
  struct list_head link;
  void *kernstack;
  u32 kernstacksize;
  struct vm_map *vmmap;
  u8 state;
  u32 flags;
  u32 pid;
  void *waitcause;
};

void task_init(void);
void kernstack_setaddr(void);
struct task *kernel_task_new(void *eip, int intenable);
void task_run(struct task *t);
void task_sched(void);
void task_sleep(void *cause);
void task_wakeup(void *cause);
void defer_exec(void (*func)(void *), void *arg, int priority, int delay);
