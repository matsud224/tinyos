#include <stdint.h>
#include <stddef.h>
#include <kern/kernasm.h>
#include <kern/pagetbl.h>
#include <kern/params.h>
#include <kern/vga.h>
#include <kern/page.h>

#define PDE_PRESENT		 		0x1
#define PDE_RW				 		0x2
#define PDE_USER				 	0x4
#define PDE_WRITETHRU	 		0x8
#define PDE_CACHE_DISABLE	0x10
#define PDE_ACCESS				0x20
#define PDE_SIZE_4MB			0x80
#define PDE_GLOBAL				0x100

#define PTE_PRESENT		 		0x1
#define PTE_RW				 		0x2
#define PTE_USER				 	0x4
#define PTE_WRITETHRU	 		0x8
#define PTE_CACHE_DISABLE	0x10
#define PTE_ACCESS				0x20
#define PTE_DIRTY					0x40
#define PTE_GLOBAL				0x100


static u32 *kernspace_pdt; //over 0xc0000000

u32 *procpdt_new() {
  u32 *pdt = get_zeropage();
  //fill kernel space page diectory entry
  for(int i = 0; i < 1024; i++)
    pdt[i] = kernspace_pdt[i];
  return pdt;
}

void pagetbl_init() {
  //setup kernel space
  kernspace_pdt = get_zeropage();
  //kernel space straight mapping(896MB)
  int st_start_index = KERN_VMEM_ADDR / 0x400000;
  int st_end_index = st_start_index + (KERN_STRAIGHT_MAP_SIZE/0x400000);
  u32 addr = 0x0; 
  for(int i = st_start_index; i < st_end_index;
        i++, addr += 0x400000){
    kernspace_pdt[i] = addr | PDE_PRESENT | PDE_RW | PDE_SIZE_4MB;
  }
  //kernel space virtual area
  for(int i = st_end_index; i < 1024; i++) {
    u32 *pt = get_zeropage();
    kernspace_pdt[i] = KERN_VMEM_TO_PHYS((u32)pt) | PDE_PRESENT | PDE_RW;
  }

  flushtlb(kernspace_pdt);
}

void pagetbl_add_mapping(u32 *pdt, u32 vaddr, u32 paddr) {
  int pdtindex = vaddr>>22;
  int ptindex = (vaddr>>12) & 0x3ff;
  if((pdt[pdtindex] & PDE_PRESENT) == 0) {
    pdt[pdtindex] = KERN_VMEM_TO_PHYS((u32)get_zeropage()) | PDE_PRESENT | PDE_RW;
  }

  u32 *pt = (u32 *)(PHYS_TO_KERN_VMEM(pdt[pdtindex] & ~0xfff));
  pt[ptindex] = (KERN_VMEM_TO_PHYS(paddr) & ~0xfff) | PTE_PRESENT | PTE_RW;
}
