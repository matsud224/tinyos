#include <kern/trap.h>
#include <kern/kernasm.h>
#include <kern/vmem.h>
#include <kern/pagetbl.h>
#include <kern/task.h>
#include <kern/kernlib.h>

struct trap_stack {
  u32 errcode;
  u32 eip;
  u32 cs;
  u32 eflags;
};

void gpe_isr() {
  printf("General Protection Exception in task#%d\n", current->pid);
  task_exit();
}

void pf_isr(u32 addr, u32 eip) {
  printf("\nPage fault addr = 0x%x (eip = 0x%x)\n", addr, eip);
  struct vm_area *varea = vm_findarea(current->vmmap, addr);
  if(varea == NULL) {
    printf("Segmentation Fault in task#%d\n", current->pid);
    task_exit();
  } else {
    u32 paddr = varea->mapper->ops->request(varea->mapper, addr - varea->start);
    pagetbl_add_mapping(current->regs.cr3, addr, paddr);
  }
}

void syscall_isr(u32 eax, u32 ebx, u32 ecx, u32 edx, u32 esi, u32 edi) {
  printf("syscall: %x,%x,%x,%x,%x,%x\n", eax, ebx, ecx, edx, esi, edi);
  task_yield();
}
