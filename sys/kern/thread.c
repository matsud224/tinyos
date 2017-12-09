#include <kern/thread.h>
#include <kern/pit.h>
#include <kern/page.h>
#include <kern/pagetbl.h>
#include <kern/gdt.h>
#include <kern/kernasm.h>
#include <kern/vmem.h>
#include <kern/kernlib.h>
#include <kern/chardev.h>
#include <kern/netdev.h>
#include <kern/timer.h>
#include <kern/lock.h>

#include <net/socket/socket.h>
#include <net/inet/inet.h>
#include <net/util.h>

static struct tss tss;

struct thread *current = NULL;
static u32 pid_next = 0; 

static struct list_head run_queue;
static struct list_head wait_queue;

void thread_a(void *arg) {
  printf("arg = %d\n", (int)arg);
}

void thread_b(void *arg) {
  if(fs_mountroot("fat32", (void *)0) < 0) {
    puts("mountroot failed.");
    while(1);
  } else {
    puts("mount ok");
  }
  struct inode *ino = fs_nametoi("/foo/../foo/rfc3514.txt");
  if(ino == NULL) {
    puts("nametoi failed.");
    while(1);
  }
  printf("file found inode=%x\n", ino);
  vm_add_area(current->vmmap, 0x20000, PAGESIZE*2, inode_mapper_new(ino, 0), 0);

  for(u32 addr=0x20200; addr<0x20300; addr++) {
    printf("%c", *(char*)addr);
    if(*(char*)addr == '\0')
      break;
  }
}

void thread_idle(void *arg) {
  while(1)
    cpu_halt();
}

void thread_test(void *arg) {
  struct socket *sock;
  struct sockaddr_in addr;

  char buf[2048];

  sock = socket(PF_INET, SOCK_DGRAM);
  if(sock == NULL) {
    puts("failed to open socket");
    return;
  }

  addr.family = PF_INET;
  addr.port = hton16(12345);
  addr.addr = INADDR_ANY;

  bind(sock, (struct sockaddr *)&addr);

  memset(buf, 0, sizeof(buf));
  while(1) {
    int len = recv(sock, buf, sizeof(buf), 0);
    buf[len] = '\0';
    printf("%s\n", buf);
  }

  close(sock);
}

void thread_echo(void *arg) {
  struct socket *sock;
  struct sockaddr_in addr;

  sock = socket(PF_INET, SOCK_DGRAM);
  if(sock == NULL) {
    puts("failed to open socket");
    return;
  }

  addr.family = PF_INET;
  addr.port = hton16(54321);
  addr.addr = IPADDR(192,168,4,1);

  u8 data;
  while(1) {
    if(chardev_read(0, &data, 1) == 1) {
      chardev_write(0, &data, 1);
      if(sendto(sock, &data, 1, 0, (struct sockaddr *)&addr) < 0)
        puts("sendto failed");
    }
  }

  close(sock);
}

void dispatcher_init() {
  current = NULL;
  list_init(&run_queue);
  list_init(&wait_queue);

  bzero(&tss, sizeof(struct tss));
  tss.ss0 = GDT_SEL_DATASEG_0;

  gdt_init();
  gdt_settssbase(&tss);
  ltr(GDT_SEL_TSS);

  thread_run(kthread_new(thread_a, 3));
  thread_run(kthread_new(thread_b, NULL));
  thread_run(kthread_new(thread_idle, NULL));
  thread_run(kthread_new(thread_echo, NULL));
  thread_run(kthread_new(thread_test, NULL));
}

void dispatcher_run() {
  jmpto_current();
}

void kstack_setaddr() {
  tss.esp0 = (u32)((u8 *)(current->kstack) + current->kstacksize);
}


void thread_exit(void);

struct thread *kthread_new(void (*func)(void *), void *arg) {
  struct thread *t = malloc(sizeof(struct thread));
  bzero(t, sizeof(struct thread));
  t->vmmap = vm_map_new();
  t->state = TASK_STATE_RUNNING;
  t->pid = pid_next++;
  t->regs.cr3 = procpdt_new();
  //prepare kernel stack
  t->kstack = page_alloc();
  bzero(t->kstack, PAGESIZE);
  t->kstacksize = PAGESIZE;
  t->regs.esp = (u32)((u8 *)(t->kstack) + t->kstacksize - 4);
  *(u32 *)t->regs.esp = arg;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = (u32)thread_exit;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = func;
  t->regs.esp -= 4*5;
  *(u32 *)t->regs.esp = 0x200; //initial eflags(IF=1)
  return t;
}

void thread_run(struct thread *t) {
  t->state = TASK_STATE_RUNNING;
  if(current == NULL)
    current = t;
  else
    list_pushback(&(t->link), &run_queue);
}

static void thread_free(struct thread *t) {
  page_free(t->kstack);
  free(t);
}

void thread_sched() {
  //printf("sched: nextpid=%d esp=%x\n", current->pid, current->regs.esp);
  switch(current->state) {
  case TASK_STATE_RUNNING:
    list_pushback(&(current->link), &run_queue);
    break;
  case TASK_STATE_WAITING:
    list_pushback(&(current->link), &wait_queue);
    break;
  case TASK_STATE_EXITED:
    thread_free(current);
    break;
  }
  struct list_head *next = list_pop(&run_queue);
  if(next == NULL) {
    puts("no thread!");
    while(1)
      cpu_halt();
  }
  current = container_of(next, struct thread, link);
}

void thread_yield() {
  IRQ_DISABLE 
  _thread_yield();
  IRQ_RESTORE
}

void thread_sleep(void *cause) {
  //printf("thread#%d sleep\n", current->pid);
  current->state = TASK_STATE_WAITING;
  current->waitcause = cause;
  thread_yield();
}

void thread_wakeup(void *cause) {
  int wake = 0;
  struct list_head *h, *tmp;
  list_foreach_safe(h, tmp, &wait_queue) {
    struct thread *t = container_of(h, struct thread, link); 
    if(t->waitcause == cause) {
      //printf("thread#%d wakeup\n", t->pid);
      wake = 1;
      t->state = TASK_STATE_RUNNING;
      list_remove(h);
      list_pushfront(h, &run_queue);
    }
  }
}

void thread_start_alarm(void *cause, u32 expire) {
  timer_start(expire, thread_wakeup, cause);
}

void thread_exit() {
  //printf("thread#%d exit\n", current->pid);
  current->state = TASK_STATE_EXITED;
  thread_yield();
}