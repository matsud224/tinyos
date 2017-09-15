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

#define HALT while(1)

KERNENTRY void kernel_main(void) {
  a20_enable();
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
    HALT;
  }
  struct inode *ino = fs_nametoi("/etc/oldhelp/fs");
  if(ino == NULL) {
    puts("nametoi failed.");
    HALT;
  }
  vm_add_area(current_vmmap, 0x20000, PAGESIZE*2, inode_mapper_new(ino, 0), 0);
  for(uint32_t addr=0x20f00; addr<0x21100; addr++) {
    printf("%c", *(char*)addr);
  }
  HALT;
}


