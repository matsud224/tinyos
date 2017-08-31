#include "trap.h"
#include "vga.h"
#include <stdint.h>

void gpe_inthandler() {
  puts("General Protection Exception");
  while(1);
}
