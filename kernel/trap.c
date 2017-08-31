#include "trap.h"
#include "vga.h"
#include <stdint.h>

void gpe_inthandler() {
  puts("General Protected Exception\n");
  while(1);
}
