#pragma once

#include "vmem.h"
#include "list.h"
#include <stdint.h>
#include <stddef.h>

extern struct task *current;

struct tss {
  uint16_t backlink;	uint16_t f1;
  uint32_t esp0;
  uint16_t ss0;				uint16_t f2;
  uint32_t esp1;
  uint16_t ss1;				uint16_t f3;
  uint32_t esp2;
  uint16_t ss2;				uint16_t f4;
  uint32_t cr3;
  uint32_t eip;
  uint32_t eflags;
  uint32_t eax;
  uint32_t ecx;
  uint32_t edx;
  uint32_t ebx;
  uint32_t esp;
  uint32_t ebp;
  uint32_t esi;
  uint32_t edi;
  uint16_t es;				uint16_t f5;
  uint16_t cs;				uint16_t f6;
  uint16_t ss;				uint16_t f7;
  uint16_t ds;				uint16_t f8;
  uint16_t fs;				uint16_t f9;
  uint16_t gs;				uint16_t f10;
  uint16_t ldt;				uint16_t f11;
  uint16_t t;					uint16_t iobase;
};

struct task_state {
  uint32_t esp;
  uint32_t cr3;
};

#define TASK_STATE_RUNNING	0
#define TASK_STATE_WAITING	1

struct task {
  struct task_state regs;
  struct list_head link;
  void *kernstack;
  uint32_t kernstacksize;
  struct vm_map *vmmap;
  uint8_t state;
  uint32_t flags;
  uint32_t pid;
  struct task *next;
  void *waitcause;
};


void task_init(void);
void kernstack_setaddr(void);
struct task *kernel_task_new(void *eip);
void task_run(struct task *t);
void task_sched(void);
void task_sleep(void *cause);
void task_wakeup(void *cause);
