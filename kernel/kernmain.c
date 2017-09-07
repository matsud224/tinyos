#include <stddef.h>
#include <stdint.h>
#include "common.h"
#include "vga.h"
#include "pci.h"
#include "trap.h"
#include "page.h"
#include "idt.h" 
#include "pic.h" 
#include "pit.h" 
#include "malloc.h" 
#include "kernasm.h"
#include "pagetbl.h"
#include "vmem.h"
#include "ide.h"

KERNENTRY void kernel_main(void) {
	vga_init();
	puts("hello, world!");
  page_init();
  printf("%d MB free\n", (page_getnfree()*4)/1024);
  idt_init();
  pic_init();
  idt_register(13, IDT_INTGATE, gpe_isr);
  idt_register(14, IDT_INTGATE, pf_isr);
  pagetbl_init();
  //vmem_init();
  //pit_init();
  sti();
  pci_printinfo();
  ide_init();
  int err;
  if((err=ide_ata_access(0, 2, 0, 1, 0, 0)) < 0)
    printf("ata_error:%d\n", err);
  puts("bye");
  while(1);
}


