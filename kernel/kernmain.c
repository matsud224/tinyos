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
  char *page2 = page_alloc();
  struct io_request *req1 = NULL;
  struct io_request *req2 = NULL;
if(page1 == NULL)
  puts("page1 error!");
if(page2 == NULL)
  puts("page2 error!");
  req1 = ide_request(0, 0, 2, (void *)((uint32_t)page1-KERNSPACE_ADDR), 0);
  req2 = ide_request(1, 0, 4, (void *)((uint32_t)page2-KERNSPACE_ADDR), 0);
  ioreq_wait(req2);
  puts("bye");
  if(ioreq_checkerror(req2))
    puts("error occered");
  else
    puts("success");
    for(int i=0; i<2048; i++)
      printf("%c", page2[i]);
  while(1);
}


