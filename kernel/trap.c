#include "trap.h"
#include "vga.h"
#include <stdint.h>
#include "kernasm.h"
#include "vmem.h"
#include "pagetbl.h"
#include "task.h"

void gpe_isr() {
  puts("General Protection Exception");
  while(1);
}

void pf_isr(uint32_t addr) {
  printf("\nPage fault! addr = 0x%x\n", addr);
  printf("eip=%x, esp=%x\n", current->regs.eip, current->regs.esp);
  struct vm_area *varea = vm_findarea(current->vmmap, addr);
  if(varea == NULL) {
    puts("Segmentation fault!\n");
    while(1);
  } else {
    uint32_t paddr = varea->mapper->ops->request(varea->mapper, addr - varea->start);
    uint8_t *page = page_alloc();
    page[0] = 'h';
    page[1] = 'e';
    page[2] = 'l';
    page[3] = 'l';
    page[4] = 'o';
    page[5] = '\0';
    pagetbl_add_mapping(current->regs.cr3, addr, page);
  }
}

void syscall_isr() {
  puts("syscall");
  while(1);
}
