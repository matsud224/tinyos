#include "trap.h"
#include "kernasm.h"
#include "vmem.h"
#include "pagetbl.h"
#include "task.h"
#include "kernlib.h"

void gpe_isr() {
  printf("General Protection Exception in task#%d\n", current->pid);
  task_exit();
}

void pf_isr(u32 addr) {
  printf("\nPage fault! addr = 0x%x\n", addr);
  //printf("eip=%x, esp=%x\n", current->regs.eip, current->regs.esp);
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
