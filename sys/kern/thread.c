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


static struct tss tss;

struct thread *current = NULL;
static struct thread *thread_tbl[MAX_THREADS];
static pid_t pid_last = INVALID_PID+1;

static struct list_head run_queue[MAX_PRIORITY];
static struct list_head wait_queue;


extern void thread_main(void *arg UNUSED);


void thread_idle(UNUSED void *arg) {
  while(1)
    cpu_halt();
}

void thread_set_priority(u32 priority) {
  if(priority < MAX_PRIORITY)
    current->priority = priority;
}

void dispatcher_init() {
  for(int i=0; i<MAX_THREADS; i++)
    thread_tbl[i] = NULL;

  for(int i=0; i<MAX_PRIORITY; i++)
    list_init(&run_queue[i]);

  current = NULL;
  list_init(&wait_queue);

  bzero(&tss, sizeof(struct tss));
  tss.ss0 = GDT_SEL_DATASEG_0;

  gdt_init();
  gdt_settssbase(&tss);
  ltr(GDT_SEL_TSS);

  thread_run(kthread_new(thread_idle, NULL, "idle", PRIORITY_IDLE));
  thread_run(kthread_new(thread_main, NULL, "main", PRIORITY_USER));
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

struct thread *kthread_new(void (*func)(void *), void *arg, const char *name, u32 priority) {
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
  t->kstack = get_zeropage(KSTACK_SIZE);
  t->kstacksize = KSTACK_SIZE;
  t->regs.esp = (u32)((u8 *)(t->kstack) + t->kstacksize - 4);
  *(u32 *)t->regs.esp = (u32)arg;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = (u32)thread_exit_with_error;
  t->regs.esp -= 4;
  *(u32 *)t->regs.esp = (u32)func;
  t->regs.esp -= 4*5;
  *(u32 *)t->regs.esp = 0x200; //initial eflags(IF=1)

  t->priority = priority;

  thread_tbl[t->pid] = t;

  return t;
}


static char *next_string(char *ptr) {
  while(*ptr++ != '\0');
  return ptr;
}

int thread_exec_in_usermode(const char *path, char *const argv[], char *const envp[]) {
#define ARGSBUFSIZE 1024
  static char argsbuf[ARGSBUFSIZE];

  //prepare argv&envp
  int argc = 0, envc = 0;
  int bufremain = ARGSBUFSIZE;
  char *bufptr = argsbuf;
  for(const char **ptr = argv; ptr != NULL && *ptr != NULL; ptr++, argc++) {
    size_t len = strnlen(*ptr, ARGSBUFSIZE);
    if(bufremain >= len+1) {
      memcpy(bufptr, *ptr, len);
      bufptr += len;
      *bufptr++ = '\0';
      bufremain -= len+1;
    } else {
      return -1;
    }
  }
  for(const char **ptr = envp; ptr != NULL && *ptr != NULL; ptr++, envc++) {
    size_t len = strnlen(*ptr, ARGSBUFSIZE);
    if(bufremain >= len+1) {
      memcpy(bufptr, *ptr, len);
      bufptr += len;
      *bufptr++ = '\0';
      bufremain -= len+1;
    } else {
      return -1;
    }
  }

  int strstart, tablestart;
  strstart = bufremain;
  bufremain = align(bufremain, sizeof(char **)) + sizeof(char **);
  int table_and_argc_size = (argc+envc+2)*sizeof(char **) + sizeof(int);
  if(bufremain < table_and_argc_size) {
    return -1;
  }
  bufremain -= table_and_argc_size;
  tablestart = bufremain;

  struct file *f = open(path, O_RDONLY);
  if(f == NULL)
    return -1;

  if(!elf32_is_valid_exec(f)) {
    return -1;
  }

  lseek(f, 0, SEEK_SET);

  current->name = strdup(path);

  vm_map_free(current->vmmap);
  current->vmmap = vm_map_new();
  current->regs.cr3 = pagetbl_new();

  void *brk;
  int (*entrypoint)(void) = elf32_load(f, &brk);
  close(f);

  //prepare user space stack
  struct mapper *m = anon_mapper_new();
  current->user_stack_bottom = USER_STACK_BOTTOM;
  current->user_stack_top = USER_STACK_BOTTOM - USER_STACK_INITIAL_SIZE;
  vm_add_area(current->vmmap, current->user_stack_top, USER_STACK_INITIAL_SIZE, m, 0);

  //prepare argv&envp(continue)
  void *stackpage = (char *)anon_mapper_add_page(m, USER_STACK_BOTTOM - PAGESIZE);
  char *strptr = (u8 *)stackpage + strstart;
  char **tableptr = (u8 *)stackpage + tablestart - sizeof(int);
  memcpy(strptr, argsbuf, ARGSBUFSIZE - strstart);
  *(int *)tableptr = argc; tableptr++;

  for(int i=0; i<argc; i++) {
    *tableptr++ = USER_STACK_BOTTOM - PAGESIZE + ((u32)strptr - (u32)stackpage);
    strptr = next_string(strptr);
  }
  *tableptr++ = NULL;
  for(int i=0; i<envc; i++) {
    *tableptr++ = USER_STACK_BOTTOM - PAGESIZE + (u32)strptr - (u32)stackpage;
    strptr = next_string(strptr);
  }
  *tableptr++ = NULL;

  current->brk = pagealign((u32)brk+(PAGESIZE-1));

  flushtlb(current->regs.cr3);

  jmpto_userspace(entrypoint, (void *)(USER_STACK_BOTTOM - PAGESIZE + tablestart - sizeof(int)));
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
  if(t->curdir)
    vnode_hold(t->curdir);
  t->pid = childpid;
  t->ppid = current->pid;
  t->regs.cr3 = pagetbl_dup_for_fork((paddr_t)current->regs.cr3);
  flushtlb(current->regs.cr3);

  //prepare kernel stack
  t->kstack = get_zeropage(KSTACK_SIZE);
  t->kstacksize = KSTACK_SIZE;
  u32 current_esp = getesp();
  u32 distance = current_esp - (u32)current->kstack;
  memcpy(t->kstack + distance, current_esp, t->kstacksize - distance);
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
  t->num_pfs = 0;
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
    list_pushback(&(t->link), &run_queue[t->priority]);
}

static void thread_free(struct thread *t) {
  thread_tbl[t->pid] = NULL;

  for(int i=0; i<MAX_THREADS; i++) {
    if(thread_tbl[i] && thread_tbl[i]->ppid == t->pid) {
      thread_tbl[i]->ppid = INVALID_PID;
      if(thread_tbl[i]->state == TASK_STATE_ZOMBIE)
        thread_free(thread_tbl[i]);
    }
  }

  page_free(t->kstack);
  pagetbl_free(t->regs.cr3);
  free(t);
}

void thread_sched() {
  switch(current->state) {
  case TASK_STATE_RUNNING:
    list_pushback(&(current->link), &run_queue[current->priority]);
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

  struct list_head *next = NULL;
  for(int i=0; i<MAX_PRIORITY; i++) {
    next = list_pop(&run_queue[i]);
    if(next != NULL)
      break;
  }
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

void thread_check_signal() {
  if(current->signal > 0)
    thread_exit_with_error();
}

void thread_sleep(const void *cause) {
  //printf("thread#%d (%s) sleep for %x\n", current->pid, GET_THREAD_NAME(current), cause);
  current->state = TASK_STATE_WAITING;
  current->waitcause = cause;
  thread_yield();
  thread_check_signal();
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
      list_pushfront(h, &run_queue[t->priority]);
    }
  }
}

void thread_set_alarm(void *cause, u32 expire) {
  timer_start(expire, thread_wakeup, cause);
}

void thread_exit(int exit_code) {
  for(int i=0; i<MAX_FILES; i++)
    if(current->files[i])
      close(current->files[i]);

  if(current->curdir)
    vnode_release(current->curdir);

  vm_map_free(current->vmmap);

  if(thread_tbl[current->ppid]) {
    current->state = TASK_STATE_ZOMBIE;
    current->exit_code = exit_code;
    thread_wakeup(thread_tbl[current->ppid]);
  } else {
    current->state = TASK_STATE_EXITED;
  }

  printf("thread#%d (%s) exit with %d\n", current->pid, GET_THREAD_NAME(current), exit_code);

  thread_yield();
}

void thread_exit_with_error() {
  thread_exit(-1);
}

int thread_yield_pages() {
  int is_yielded = 0;
IRQ_DISABLE
  for(int i=0; i<MAX_THREADS; i++) {
    if(i != current->pid && thread_tbl[i])
      if(vm_map_yield(thread_tbl[i]->vmmap, thread_tbl[i]->regs.cr3) == 0) {
        is_yielded = 1;
        break;
      }
  }
IRQ_RESTORE
  return is_yielded ? 0 : -1;
}

int thread_chdir(const char *path) {
  struct stat stbuf;
  if(stat(path, &stbuf) || (stbuf.st_mode & S_IFMT) != S_IFDIR)
    return -1;

  vnodes_lock();
  struct vnode *newdir = name_to_vnode(path, NULL, NULL);
  if(newdir) {
    vnode_release(current->curdir);
    vnodes_unlock();
    current->curdir = newdir;
    return 0;
  }
  return -1;
}

int gettents(struct threadent *thp, size_t count) {
  size_t nfoundent = 0;
  for(int i=0; i<MAX_THREADS && count > 0; i++) {
    if(thread_tbl[i] != NULL) {
      thp[nfoundent].state = thread_tbl[i]->state;
      thp[nfoundent].flags = thread_tbl[i]->flags;
      thp[nfoundent].pid   = thread_tbl[i]->pid;
      thp[nfoundent].ppid  = thread_tbl[i]->ppid;
      char *tname = thread_tbl[i]->name;
      if(tname == NULL)
        tname = "???";
      strncpy(thp[nfoundent].name, tname, MAX_THREADNAME_LEN);
      thp[nfoundent].brk   = thread_tbl[i]->brk;
      thp[nfoundent].user_stack_size =
        thread_tbl[i]->user_stack_bottom - thread_tbl[i]->user_stack_top;
      thp[nfoundent].num_pfs = thread_tbl[i]->num_pfs;
      thp[nfoundent].num_files = 0;
      for(int j=0; j<MAX_FILES; j++)
        if(thread_tbl[i]->files[j])
          thp[nfoundent].num_files++;

      thp[nfoundent].priority = thread_tbl[i]->priority;

      nfoundent++;
      count -= sizeof(struct threadent);
    }
  }

  return nfoundent * sizeof(struct threadent);
}


int sys_execve(const char *filename, char *const argv[], char *const envp[]) {
  return thread_exec_in_usermode(filename, argv, envp);
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

  if(current->brk + incr > current->user_stack_top)
    return -1;

  u32 prev_brk = (u32)current->brk;
  u32 new_brk = pagealign((u32)current->brk + incr + (PAGESIZE-1));

  //add mapping if brk go over the page boundary.
  vm_add_area(current->vmmap, current->brk, new_brk-prev_brk, anon_mapper_new(), 0);
  current->brk = new_brk;

  return (int)prev_brk;
}

int sys_chdir(const char *path) {
  return thread_chdir(path);
}

int sys_gettents(struct threadent *thp, size_t count) {
  return gettents(thp, count);
}

int sys_kill(pid_t pid, int sig) {
  if(pid >= MAX_THREADS || thread_tbl[pid] == NULL)
    return -1;

  thread_tbl[pid]->signal = sig;
  if(thread_tbl[pid]->state == TASK_STATE_WAITING)
    thread_wakeup(thread_tbl[pid]->waitcause);

  return 0;
}
