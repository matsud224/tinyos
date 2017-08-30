#include <stddef.h>
#include <stdint.h>
#include "common.h"
#include "vga.h"
#include "phymem.h"
#include "idt.h" 


KERNENTRY void kernel_main(void) {
	vga_init();
	puts("hello, world!\n");

  phymem_init();
  printf("memory %d MB free\n", phymem_getfreepages()*4096/(1024*1024));
}


