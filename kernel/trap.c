#include "trap.h"
#include "vga.h"
#include <stdint.h>

void gpe_inthandler() {
  puts("General Protection Exception");
  while(1);
}

void pf_inthandler() {
  puts("Page Fault!");
  while(1);
}
