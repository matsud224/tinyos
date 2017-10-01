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
#include "gdt.h"
#include "task.h"
#include "fat32.h"

#define HALT while(1)

KERNENTRY void kernel_main(void) {
  a20_enable();
	vga_init();
	puts("hello, world!");
  page_init();
  printf("%d MB free\n", (page_getnfree()*4)/1024);
  idt_init();
  pic_init();
  idt_register(13, IDT_INTGATE, gpe_inthandler);
  idt_register(14, IDT_INTGATE, pf_inthandler);
  idt_register(0x80, IDT_INTGATE, syscall_inthandler);
  pagetbl_init();
  vmem_init();
  serial_init();
  //pit_init();
  pci_printinfo();
  blkdev_init();
  ide_init();
  v6fs_init();
  fat32_init();
  task_init();
  HALT;
}


