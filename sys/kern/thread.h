#pragma once

#include <kern/vmem.h>
#include <kern/list.h>
#include <stdint.h>
#include <stddef.h>

struct deferred_func;

extern struct thread *current;

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

struct thread_state {
  u32 esp;
  u32 cr3;
};

#define TASK_STATE_RUNNING	0
#define TASK_STATE_WAITING	1
#define TASK_STATE_EXITED		2

struct thread {
  struct thread_state regs; //do not move
  struct list_head link;
  void *kstack;
  u32 kstacksize;
  struct vm_map *vmmap;
  u8 state;
  u32 flags;
  u32 pid;
  void *waitcause;
  char *name;
};

void dispatcher_init(void);
void dispatcher_run(void);
void kstack_setaddr(void);
struct thread *kthread_new(void (*func)(void *), void *arg, char *name);
int thread_exec(struct inode *ino);
void thread_run(struct thread *t);
void thread_sched(void);
void thread_sleep(void *cause);
void thread_wakeup(void *cause);
void thread_yield(void);
void thread_set_alarm(void *cause, u32 expire);
struct deferred_func *defer_exec(void (*func)(void *), void *arg, int priority, int delay);
void *defer_cancel(struct deferred_func *f);
