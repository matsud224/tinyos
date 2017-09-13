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
#include "blkdev.h"
#include "ide.h"
#include "params.h"
#include "fs.h"
#include "v6fs.h"

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
  blkdev_init();
  ide_init();
  v6fs_init();
  if(fs_mountroot("v6fs", (void *)0) < 0) {
    puts("mountroot failed.");
    while(1);
  }
  puts("mountroot ok.");
  struct inode *ino = fs_nametoi("/etc/help/fc");
  if(ino == NULL) {
    puts("nametoi failed.");
    while(1);
  }
  printf("size = %d\n", ino->size);
  uint8_t *page = page_alloc();
  int cnt = fs_read(ino, page, 0, PAGESIZE);
  printf("read %d bytes\n", cnt);
  for(int i=0; i<cnt; i++)
    printf("%c", page[i]);
  while(1);
}


