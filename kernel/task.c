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

static struct tss tss;

struct task *current;
u32 pid_next = 0; 

struct list_head run_queue;
struct list_head wait_queue;

void task_a() {
  cli();
  pit_init();
  sti();
  while(1) {
    struct pktbuf_head *pkt = netdev_rx(0);
    printf("Pakcet received: %d byte\n", pkt->total);
    pktbuf_free(pkt);
  }
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
 
  u8 data;
  while(1) {
    if(chardev_read(0, &data, 1) == 1) {
      chardev_write(0, &data, 1);
    }
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
  tc = kernel_task_new(task_idle);
  current = ta;
  task_run(tb);
  task_run(tc);
  jmpto_current();
}

void kernstack_setaddr() {
  tss.esp0 = (u32)((u8 *)(current->kernstack) + current->kernstacksize);
}


struct task *kernel_task_new(void *eip) {
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
  *(u32 *)t->regs.esp = eip;
  t->regs.esp -= 4*5;
  *(u32 *)t->regs.esp = 0x200; //initial eflags(IF=1)
  t->next = NULL;
  return t;
}

void task_run(struct task *t) {
  t->state = TASK_STATE_RUNNING;
  list_pushback(&(t->link), &run_queue);
  return;
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
  }
  struct list_head *next = list_pop(&run_queue);
  if(next == NULL)
    puts("no task!");
  current = container_of(next, struct task, link);
}

void task_sleep(void *cause) {
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
      wake = 1;
      t->state = TASK_STATE_RUNNING;
      list_remove(h);
      list_pushfront(h, &run_queue);
    }
  }
}

