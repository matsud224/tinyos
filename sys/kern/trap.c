#include <kern/trap.h>
#include <kern/kernasm.h>
#include <kern/vmem.h>
#include <kern/pagetbl.h>
#include <kern/thread.h>
#include <kern/kernlib.h>
#include <kern/syscalls.h>

struct trap_stack {
  u32 errcode;
  u32 eip;
  u32 cs;
  u32 eflags;
};

void gpe_isr(int errcode) {
  printf("General Protection Exception in thread#%d (%s)\n  errorcode = %d\n", current->pid, GET_THREAD_NAME(current), errcode);
  while(1);
  thread_exit_with_error();
}

void pf_isr(vaddr_t addr, u32 eip, u32 esp, u32 eax) {
  //printf("Page fault in thread#%d (%s) addr=0x%x (eip=0x%x, esp=0x%x)\n", current->pid, GET_THREAD_NAME(current), addr, eip, esp);
  struct vm_area *varea;
try_findarea:
  varea = vm_findarea(current->vmmap, addr);
  if(varea == NULL) {
    if(addr > current->brk && addr < current->user_stack_bottom) {
      //stack auto grow
      current->user_stack_top -= USER_STACK_GROW_SIZE;
      if(current->brk < current->user_stack_top) {
        vm_add_area(current->vmmap, current->user_stack_top, USER_STACK_GROW_SIZE, anon_mapper_new(), 0);
        puts("STACK AUTO GROW");
        goto try_findarea;
      }
    }
    printf("Segmentation Fault in thread#%d (%s) addr = 0x%x (eip = 0x%x, esp = 0x%x)\n", current->pid, GET_THREAD_NAME(current), addr, eip, esp);
    thread_exit_with_error();
  } else {
    paddr_t paddr = varea->mapper->ops->request(varea->mapper, addr - varea->start);
    pagetbl_add_mapping((u32 *)current->regs.cr3, addr, paddr);
    flushtlb(current->regs.cr3);
  }
}

u32 syscall_isr(u32 eax, u32 ebx, u32 ecx, u32 edx, u32 esi, u32 edi) {
  //printf("syscall in thread#%d (%s) eax=%d,ebx=%x,ecx=%x,edx=%x,esi=%x,edi=%x\n", current->pid, GET_THREAD_NAME(current), eax, ebx, ecx, edx, esi, edi);
  if(eax >= NSYSCALLS) {
    printf("syscall#%d is invalid.\n", eax);
    thread_exit_with_error();
  }
  int ret = syscall_table[eax](ebx, ecx, edx, esi, edi);
  //printf("pid %d : return from syscall_isr eax=%d ret=%d  (%x)\n", current->pid, eax, ret, &ret);
  return ret;
}

void spurious_isr() {
  puts("spurious interrupt!");
  thread_exit_with_error();
}


