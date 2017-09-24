#include "task.h"
#include "pit.h"
#include "page.h"
#include "malloc.h"
#include "gdt.h"
#include "params.h"
#include "kernasm.h"
#include "vmem.h"
#include "kernlib.h"
#include <stdint.h>
#include <stddef.h>

static struct tss tss;

struct task *current;
uint32_t pid_next = 0; 

struct list_head run_queue;
struct list_head wait_queue;

void task_a() {
  cli();
  pit_init();
  sti();
  while(1) {
    puts("hello");
    puts("world");
  }
}

void task_b() {
  /*if(fs_mountroot("fat32", (void *)0) < 0) {
    puts("mountroot failed.");
    while(1);
  }
  struct inode *ino = fs_nametoi("/foo");
  if(ino == NULL) {
    puts("nametoi failed.");
    while(1);
  }
  puts("file found");
 */ 
  while(1) {
    puts("abcd");
    puts("efgh");
  }
}

void task_idle() {
  while(1)
    cpu_halt();
}

void task_init() {
  current = NULL;
  list_init(&run_queue);
  list_init(&wait_queue);

  bzero(&tss, sizeof(struct tss));
  tss.ss0 = GDT_SEL_DATASEG_0;

  gdt_init();
  gdt_settssbase(&tss);
  ltr(GDT_SEL_TSS);

  struct task *ta, *tb, *tc;
  ta = kernel_task_new(task_a);
  tb = kernel_task_new(task_b);
  tc = kernel_task_new(task_b);
  current = ta;
  task_run(tb);
  task_run(tc);
  rettotask();
}

void kernstack_setaddr() {
  tss.esp0 = (uint32_t)current->kernstack;
}

struct task *kernel_task_new(void *eip) {
  struct task *t = malloc(sizeof(struct task));
  bzero(t, sizeof(struct task));
  t->vmmap = vm_map_new();
  t->state = TASK_STATE_RUNNING;
  t->pid = pid_next++;
  t->regs.ds = t->regs.es = t->regs.fs
   = t->regs.gs = t->regs.ss = GDT_SEL_DATASEG_0;
  t->regs.cs = GDT_SEL_CODESEG_0;
  t->regs.eip = (uint32_t)eip;
  t->regs.eflags = 0x200;
  //prepare kernel stack
  t->kernstack = page_alloc();
  t->kernstacksize = PAGESIZE;
  t->regs.esp = (uint32_t)((uint8_t *)(t->kernstack) + t->kernstacksize);
  t->next = NULL;
  return t;
}

void task_run(struct task *t) {
  t->state = TASK_STATE_RUNNING;
  list_pushback(&(t->link), &run_queue);
  return;
}

void task_sched() {
  //printf("sched: pid=%d\n", current->pid);
  switch(current->state) {
  case TASK_STATE_RUNNING:
    list_pushback(&(current->link), &run_queue);
    break;
  case TASK_STATE_WAITING:
    list_pushback(&(current->link), &wait_queue);
    break;
  }
  struct list_head *next = list_pop(&run_queue);
  if(next == NULL)
    puts("no task!");
  current = container_of(next, struct task, link);
  //printf("sched: nextpid=%d, eip=%x, esp=%x\n", current->pid, current->regs.eip, current->regs.esp);
}

void task_sleep(void *cause) {
  current->state = TASK_STATE_WAITING;
  current->waitcause = cause;
  task_yield();
}

void task_wakeup(void *cause) {
  struct list_head *h, *tmp;
  list_foreach_safe(h, tmp, &wait_queue) {
    struct task *t = container_of(h, struct task, link); 
    if(t->waitcause == cause) {
      list_remove(h);
      list_pushfront(h, &run_queue);
    }
  }
  task_yield();
}

