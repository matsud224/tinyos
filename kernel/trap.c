#include "trap.h"
#include "vga.h"
#include <stdint.h>
#include "kernasm.h"
#include "vmem.h"
#include "pagetbl.h"

void gpe_inthandler() {
  puts("General Protection Exception");
  while(1);
}

void pf_inthandler(uint32_t errcode, uint32_t addr) {
  printf("Page fault! errcode = 0x%x, addr = 0x%x\n", errcode, addr);
  struct vm_area *varea = vm_findarea(current_vmmap, addr);
  if(varea == NULL) {
    puts("Segmentation fault!\n");
    while(1);
  } else {
    uint32_t paddr = varea->mapper->ops->request(varea->mapper->info, addr);
    pagetbl_add_mapping(current_pdt, addr, paddr);
    flushtlb();
  }
}
