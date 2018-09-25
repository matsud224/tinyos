#include <kern/thread.h>
#include <kern/pit.h>
#include <kern/page.h>
#include <kern/pagetbl.h>
#include <kern/gdt.h>
#include <kern/kernasm.h>
#include <kern/vmem.h>
#include <kern/kernlib.h>
#include <kern/timer.h>
#include <kern/elf.h>
#include <kern/file.h>
#include <kern/fs.h>


#define USER_STACK_BOTTOM ((vaddr_t)0xc0000000)
#define USER_STACK_SIZE ((size_t)0x4000)

static struct tss tss;

struct thread *current = NULL;
static struct thread *thread_tbl[MAX_THREADS];
static pid_t pid_last = INVALID_PID+1;

static struct list_head run_queue;
static struct list_head wait_queue;


extern void thread_main(void *arg UNUSED);


void thread_idle(UNUSED void *arg) {
  while(1)
    cpu_halt();
}

void dispatcher_init() {
  for(int i=0; i<MAX_THREADS; i++)
    thread_tbl[i] = NULL;

  current = NULL;
  list_init(&run_queue);
  list_init(&wait_queue);

  bzero(&tss, sizeof(struct tss));
  tss.ss0 = GDT_SEL_DATASEG_0;

  gdt_init();
  gdt_settssbase(&tss);
  ltr(GDT_SEL_TSS);
  thread_run(kthread_new(thread_idle, NULL, "idle"));
  thread_run(kthread_new(thread_main, NULL, "main"));
}

void dispatcher_run() {
  jmpto_current();
}

void kstack_setaddr() {
  tss.esp0 = (u32)((u8 *)(current->kstack) + current->kstacksize);
}

pid_t get_next_pid() {
  pid_t pid;
  for(pid = pid_last; pid < MAX_THREADS; pid++) {
    if(thread_tbl[pid] == NULL) {
      pid_last = pid+1;
      return pid;
    }
  }

  for(pid = INVALID_PID+1; pid < pid_last; pid++) {
    if(thread_tbl[pid] == NULL) {
      pid_last = pid+1;
      return pid;
    }
  }
  return INVALID_PID;
}

struct thread *kthread_new(void (*func)(void *), void *arg, const char *name) {
  pid_t pid = get_next_pid();
  if(pid == INVALID_PID)
    return NULL;

  struct thread *t = malloc(sizeof(struct thread));
  bzero(t, sizeof(struct thread));
  t->vmmap = vm_map_new();
  t->state = TASK_STATE_RUNNING;
  t->name = name;
  t->pid = pid;
  t->ppid = INVALID_PID;
  t->regs.cr3 = pagetbl_new();
  t->regs.eip = 0;
  //prepare kernel stack
  t->kstack = get_zeropage();
  t->kstacksize = PAGESIZE;
  t->regs.esp = (u32)((u8 *)(t->kstack) + t->kstacksize - 4);
  *(u32 *)t->regs.esp = (u32)arg;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = (u32)thread_exit_with_error;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = (u32)func;
  t->regs.esp -= 4*5;
  *(u32 *)t->regs.esp = 0x200; //initial eflags(IF=1)

  thread_tbl[t->pid] = t;
  return t;
}

int thread_exec_in_usermode(const char *path) {
  struct file *f = open(path, O_RDONLY);
  if(f == NULL)
    return -1;

  vm_map_free(current->vmmap);
  current->vmmap = vm_map_new();
  current->regs.cr3 = pagetbl_new();

  void *brk;
  int (*entrypoint)(void) = elf32_load(f, &brk);
  close(f);
  if(entrypoint == NULL)
    return -1;

  //prepare user space stack
  vm_add_area(current->vmmap, USER_STACK_BOTTOM-USER_STACK_SIZE, USER_STACK_SIZE, anon_mapper_new(), 0);

  current->brk = pagealign((u32)brk+(PAGESIZE-1));

  flushtlb(current->regs.cr3);

  jmpto_userspace(entrypoint, (void *)(USER_STACK_BOTTOM - 4));
  return 0; //never return here
}

int fork_main(u32 ch_esp, u32 ch_eflags, u32 ch_edi, u32 ch_esi, u32 ch_ebx, u32 ch_ebp) {
  pid_t childpid = get_next_pid();
  if(childpid == INVALID_PID)
    return NULL;

  struct thread *t = malloc(sizeof(struct thread));
  memcpy(t, current, sizeof(struct thread));
  t->vmmap = vm_map_new();
  t->state = TASK_STATE_RUNNING;
  t->pid = childpid;
  t->ppid = current->pid;
  t->regs.cr3 = pagetbl_dup_for_fork((paddr_t)current->regs.cr3);

  //prepare kernel stack
  t->kstack = get_zeropage();
  t->kstacksize = PAGESIZE;
  u32 current_esp = getesp();
  u32 distance = current_esp - (u32)current->kstack;
  memcpy(t->kstack + distance, current_esp, PAGESIZE - distance);
  //ch_esp points a return address to sys_fork
  t->regs.esp = (u32)t->kstack + (ch_esp - (u32)current->kstack);

  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = ch_ebp;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = ch_ebx;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = ch_esi;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = ch_edi;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = ch_eflags;

  for(int i=0; i<MAX_FILES; i++)
    if(current->files[i])
      t->files[i] = dup(current->files[i]);

  t->vmmap = vm_map_dup(current->vmmap);

  t->flags = current->flags;

  t->regs.eip = fork_child_epilogue;
  thread_tbl[t->pid] = t;
  thread_run(t);

  return t->pid;
}

void thread_run(struct thread *t) {
  t->state = TASK_STATE_RUNNING;
  if(current == NULL)
    current = t;
  else
    list_pushback(&(t->link), &run_queue);
}

static void thread_free(struct thread *t) {
  thread_tbl[t->pid] = NULL;

  for(int i=0; i<MAX_THREADS; i++)
    if(thread_tbl[i] && thread_tbl[i]->ppid == t->pid)
      thread_tbl[i]->ppid = INVALID_PID;

  page_free(t->kstack);
  pagetbl_free(t->regs.cr3);
  free(t);
}

void thread_sched() {
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
  case TASK_STATE_ZOMBIE:
    break;
  }
  struct list_head *next = list_pop(&run_queue);
  if(next == NULL) {
    puts("no thread!");
    while(1)
      cpu_halt();
  }
  current = container_of(next, struct thread, link);
  //printf("sched: pid=%d\n", current->pid);
}

void thread_yield() {
  IRQ_DISABLE
  _thread_yield();
  IRQ_RESTORE
}

void thread_sleep(const void *cause) {
  //printf("thread#%d (%s) sleep for %x\n", current->pid, GET_THREAD_NAME(current), cause);
  current->state = TASK_STATE_WAITING;
  current->waitcause = cause;
  thread_yield();
}

void thread_sleep_after_unlock(void *cause, mutex *mtx) {
IRQ_DISABLE
  mutex_unlock(mtx);
  thread_sleep(cause);
IRQ_RESTORE
}


void thread_wakeup(const void *cause) {
  struct list_head *h, *tmp;
  list_foreach_safe(h, tmp, &wait_queue) {
    struct thread *t = container_of(h, struct thread, link);
    if(t->waitcause == cause) {
      //printf("thread#%d (%s) wakeup for %x\n", t->pid, GET_THREAD_NAME(t), cause);
      t->state = TASK_STATE_RUNNING;
      list_remove(h);
      list_pushfront(h, &run_queue);
    }
  }
}

void thread_set_alarm(void *cause, u32 expire) {
  timer_start(expire, thread_wakeup, cause);
}

void thread_exit(int exit_code) {
  printf("thread#%d (%s) exit\n", current->pid, GET_THREAD_NAME(current));

  for(int i=0; i<MAX_FILES; i++)
    if(current->files[i])
      close(current->files[i]);

  vm_map_free(current->vmmap);

  if(thread_tbl[current->ppid]) {
    current->state = TASK_STATE_ZOMBIE;
    current->exit_code = exit_code;
    thread_wakeup(thread_tbl[current->ppid]);
  } else {
    current->state = TASK_STATE_EXITED;
  }

  thread_yield();
}

void thread_exit_with_error() {
  thread_exit(-1);
}

int thread_yield_pages() {
  for(int i=0; i<MAX_THREADS; i++) {
    if(thread_tbl[i])
      if(vm_map_yield(thread_tbl[i]->vmmap) == 0)
        return 0;
  }
  return -1;
}

int sys_execve(const char *filename, char *const argv[] UNUSED, char *const envp[] UNUSED) {
  return thread_exec_in_usermode(filename);
}

int sys_fork(void) {
  return fork_prologue(fork_main);
}

int sys_wait(int *status) {
  while(1) {
    for(int i=0; i<MAX_THREADS; i++) {
      struct thread *th = thread_tbl[i];
      if(th && th->state == TASK_STATE_ZOMBIE
          && th->ppid == current->pid) {
        if(status)
          *status = th->exit_code;
        pid_t child_pid = th->pid;
        thread_free(th);
        return child_pid;
      }
    }

    thread_sleep(current);
  }
  return -1;
}

int sys_sbrk(int incr) {
  if(incr < 0)
    return -1;
  else if(incr == 0)
    return current->brk;

  if(current->brk + incr > current->regs.esp)
    return -1;

  u32 prev_brk = (u32)current->brk;
  u32 new_brk = pagealign((u32)current->brk + incr + (PAGESIZE-1));

  //add mapping if brk go over the page boundary.
  vm_add_area(current->vmmap, current->brk, new_brk-prev_brk, anon_mapper_new(), 0);
  current->brk = new_brk;

  return (int)prev_brk;
}

