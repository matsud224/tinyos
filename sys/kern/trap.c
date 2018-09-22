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
  printf("General Protection Exception in thread#%d, errorcode = %d\n", current->pid, errcode);
  while(1);
  thread_exit();
}

void pf_isr(vaddr_t addr, u32 eip, u32 esp) {
  //printf("\nPage fault addr = 0x%x (eip = 0x%x, esp = 0x%x)\n", addr, eip, esp);
  struct vm_area *varea = vm_findarea(current->vmmap, addr);
  if(varea == NULL) {
    printf("Segmentation Fault in thread#%d ... addr = 0x%x (eip = 0x%x, esp = 0x%x)\n", current->pid, addr, eip, esp);
    while(1);
    thread_exit();
  } else {
    u32 paddr = (u32)(varea->mapper->ops->request(varea->mapper, addr - varea->start));
    pagetbl_add_mapping((u32 *)current->regs.cr3, addr, paddr);
  }
}

u32 syscall_isr(u32 eax, u32 ebx, u32 ecx, u32 edx, u32 esi, u32 edi) {
  //printf("syscall: %d,%x,%x,%x,%x,%x\n", eax, ebx, ecx, edx, esi, edi);
  if(eax >= NSYSCALLS) {
    printf("syscall#%d is invalid.\n", eax);
    thread_exit();
  }
  return syscall_table[eax](ebx, ecx, edx, esi, edi);
}

void spurious_isr() {
  puts("spurious interrupt!");
  while(1);
  thread_exit();
}


