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
#include "params.h"

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
  vmem_init();
  //pit_init();
  sti();
  pci_printinfo();
  ide_init();
  char *page1 = page_alloc();
  struct io_request *req = NULL;
  req=ide_request(0, 1, 2, (void *)((uint32_t)page1-KERNSPACE_ADDR), 0);
  ioreq_wait(req);
  puts("bye");
  if(ioreq_checkerror(req))
    puts("error occered");
  else
    for(int i=0; i<1024; i++)
      printf("%c", page1[i]);

  while(1);
}


