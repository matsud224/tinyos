#include "trap.h"
#include "vga.h"
#include <stdint.h>
#include "kernasm.h"

void gpe_inthandler() {
  puts("General Protection Exception");
  while(1);
}

void pf_inthandler(uint32_t errcode, uint32_t addr) {
  printf("Page fault! errcode = 0x%x, addr = 0x%x\n", errcode, addr);
  while(1);
  //vm_findarea(current_vmmap, addr);
}
