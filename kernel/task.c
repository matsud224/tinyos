#include "task.h"
#include "pit.h"
#include "page.h"
#include "pagetbl.h"
#include "gdt.h"
#include "kernasm.h"
#include "vmem.h"
#include "kernlib.h"
#include "chardev.h"
#include "netdev.h"
#include "timer.h"

static struct tss tss;

struct task *current = NULL;
u32 pid_next = 0; 

struct list_head run_queue;
struct list_head wait_queue;

void task_a() {
  cli();
  netdev_init();
  if(rtl8139_probe())
    puts("RTL8139 found");
  else
    puts("RTL8139 not found");

  pit_init();
  sti();
}

void task_b() {
  if(fs_mountroot("fat32", (void *)0) < 0) {
    puts("mountroot failed.");
    while(1);
  } else {
    puts("mount ok");
  }
  struct inode *ino = fs_nametoi("/verilog/../verilog/counter/counter.v");
  if(ino == NULL) {
    puts("nametoi failed.");
    while(1);
  }
  printf("file found inode=%x\n", ino);
  vm_add_area(current->vmmap, 0x20000, PAGESIZE*2, inode_mapper_new(ino, 0), 0);

  /*for(u32 addr=0x20000; addr<0x20200; addr++) {
    printf("%c", *(char*)addr);
    if(*(char*)addr == '\0')
      break;
  }*/
}

void task_idle() {
  while(1)
    cpu_halt();
}

void task_echo() {
  u8 data;
  while(1) {
    if(chardev_read(0, &data, 1) == 1) {
      chardev_write(0, &data, 1);
    }
  }
}

struct deferred_func {
  struct list_head link;
  void (*func)(void *);
  void *arg;
};

struct list_head deferred_list;

void defer_exec(void (*func)(void *), void *arg, int priority) {
  struct deferred_func *f = malloc(sizeof(struct deferred_func));
  f->func = func;
  f->arg = arg;
  if(priority == 0)
    list_pushfront(f, &deferred_list);
  else
    list_pushback(f, &deferred_list);

  task_wakeup(&deferred_list);
}

void task_deferred() {
  while(1) {
    struct list_head *item;
    while((item=list_pop(&deferred_list)) == NULL)
      task_sleep(&deferred_list);
    struct deferred_func *f = container_of(item, struct deferred_func, link);
    (f->func)(f->arg);
    free(f);
  }
}

void timer_call(void *arg ) {
  puts("hello,world!");
  timer_start(1*SEC, timer_call, NULL);
}

void task_init() {
  current = NULL;
  list_init(&run_queue);
  list_init(&wait_queue);
  list_init(&deferred_list);

  bzero(&tss, sizeof(struct tss));
  tss.ss0 = GDT_SEL_DATASEG_0;

  gdt_init();
  gdt_settssbase(&tss);
  ltr(GDT_SEL_TSS);

  timer_start(20, timer_call, NULL);

  task_run(kernel_task_new(task_a, 0));
  task_run(kernel_task_new(task_b, 1));
  task_run(kernel_task_new(task_idle, 1));
  task_run(kernel_task_new(task_deferred, 1));
  task_run(kernel_task_new(task_echo, 1));
  jmpto_current();
}

void kernstack_setaddr() {
  tss.esp0 = (u32)((u8 *)(current->kernstack) + current->kernstacksize);
}


void task_exit(void);
struct task *kernel_task_new(void *eip, int intenable) {
  struct task *t = malloc(sizeof(struct task));
  bzero(t, sizeof(struct task));
  t->vmmap = vm_map_new();
  t->state = TASK_STATE_RUNNING;
  t->pid = pid_next++;
  t->regs.cr3 = new_procpdt();
  //prepare kernel stack
  t->kernstack = page_alloc();
  bzero(t->kernstack, PAGESIZE);
  t->kernstacksize = PAGESIZE;
  t->regs.esp = (u32)((u8 *)(t->kernstack) + t->kernstacksize - 4);
  *(u32 *)t->regs.esp = (u32)task_exit;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = eip;
  t->regs.esp -= 4*5;
  *(u32 *)t->regs.esp = intenable?0x200:0; //initial eflags(IF=1)
  return t;
}

void task_run(struct task *t) {
  t->state = TASK_STATE_RUNNING;
  if(current == NULL)
    current = t;
  else
    list_pushback(&(t->link), &run_queue);
}

void freetask(struct task *t) {
  page_free(t->kernstack);
  free(t);
}

void task_sched() {
  //printf("sched: nextpid=%d esp=%x\n", current->pid, current->regs.esp);
  switch(current->state) {
  case TASK_STATE_RUNNING:
    list_pushback(&(current->link), &run_queue);
    break;
  case TASK_STATE_WAITING:
    list_pushback(&(current->link), &wait_queue);
    break;
  case TASK_STATE_EXITED:
    freetask(current);
    break;
  }
  struct list_head *next = list_pop(&run_queue);
  if(next == NULL) {
    puts("no task!");
    while(1)
      cpu_halt();
  }
  current = container_of(next, struct task, link);
}

void task_sleep(void *cause) {
  //printf("task#%d sleep\n", current->pid);
  current->state = TASK_STATE_WAITING;
  current->waitcause = cause;
  task_yield();
}

void task_wakeup(void *cause) {
  int wake = 0;
  struct list_head *h, *tmp;
  list_foreach_safe(h, tmp, &wait_queue) {
    struct task *t = container_of(h, struct task, link); 
    if(t->waitcause == cause) {
  //printf("task#%d wakeup\n", t->pid);
      wake = 1;
      t->state = TASK_STATE_RUNNING;
      list_remove(h);
      list_pushfront(h, &run_queue);
    }
  }
}

void task_exit() {
  printf("task#%d exit\n", current->pid);
  current->state = TASK_STATE_EXITED;
  task_yield();
}
