#pragma once

#include <kern/vmem.h>
#include <kern/list.h>
#include <kern/file.h>
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
  u32 eip;
};

#define TASK_STATE_RUNNING	0
#define TASK_STATE_WAITING	1
#define TASK_STATE_EXITED		2
#define TASK_STATE_ZOMBIE		3

struct thread {
  struct thread_state regs; //do not move from here
  struct list_head link;
  void *kstack;
  size_t kstacksize;
  struct vm_map *vmmap;
  u8 state;
  u32 flags;
  pid_t pid;
  pid_t ppid;
  int exit_code;
  const void *waitcause;
  struct file *files[MAX_FILES];
  void *brk;
  char *name;
};

#define GET_THREAD_NAME(th) ((th)->name?(th)->name:"???")

void dispatcher_init(void);
void dispatcher_run(void);
void kstack_setaddr(void);
struct thread *kthread_new(void (*func)(void *), void *arg, const char *name);
int thread_exec(const char *path);
void thread_run(struct thread *t);
void thread_sched(void);
void thread_sleep(const void *cause);
void thread_wakeup(const void *cause);
void thread_yield(void);
void thread_set_alarm(void *cause, u32 expire);
void thread_exit(int exit_code);
void thread_exit_with_error(void);
struct deferred_func *defer_exec(void (*func)(void *), void *arg, int priority, int delay);
void *defer_cancel(struct deferred_func *f);

int sys_execve(const char *filename, char *const argv[], char *const envp[]);
int sys_fork(void);
int sys_sbrk(int incr);
