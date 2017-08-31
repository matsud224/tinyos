#include <stddef.h>
#include <stdint.h>
#include "common.h"
#include "vga.h"
#include "trap.h"
#include "phymem.h"
#include "idt.h" 
#include "pic.h" 
#include "pit.h" 
#include "kernasm.h"

KERNENTRY void kernel_main(void) {
	vga_init();
	puts("hello, world!");
  phymem_init();
  printf("%d MB free\n", phymem_getfreepages()*4096/(1024*1024));

  idt_init();
  pic_init();
  idt_register(13, IDT_INTGATE, gpe_isr);
  pit_init();
  sti();
  //gengpe();
}


