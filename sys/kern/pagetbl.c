#include <stdint.h>
#include <stddef.h>
#include <kern/kernasm.h>
#include <kern/pagetbl.h>
#include <kern/params.h>
#include <kern/vga.h>
#include <kern/page.h>
#include <kern/thread.h>

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

#define TOTAL_NUM_PDE     (PAGESIZE >> 2)
#define TOTAL_NUM_PTE     (PAGESIZE >> 2)
static u32 KERN_PDE_START;

static u32 *kernspace_pdt; //beyond 0xc0000000

void pagetbl_init() {
  //setup kernel space
  kernspace_pdt = get_zeropage();
  //kernel space straight mapping(896MB)
  int st_start_index = KERN_VMEM_ADDR / 0x400000; //0x400000 = 4MB
  KERN_PDE_START = st_start_index;
  int st_end_index = st_start_index + (KERN_STRAIGHT_MAP_SIZE/0x400000);
  vaddr_t addr = 0x0;
  for(int i = st_start_index; i < st_end_index;
        i++, addr += 0x400000){
    kernspace_pdt[i] = addr | PDE_PRESENT | PDE_RW | PDE_SIZE_4MB | PDE_USER;
  }
  //kernel space virtual area
  for(int i = st_end_index; i < TOTAL_NUM_PDE; i++) {
    u32 *pt = get_zeropage();
    kernspace_pdt[i] = KERN_VMEM_TO_PHYS((vaddr_t)pt) | PDE_PRESENT | PDE_RW | PDE_USER;
  }

  flushtlb(KERN_VMEM_TO_PHYS(kernspace_pdt));
}


paddr_t pagetbl_new() {
  u32 *pdt = get_zeropage();
  //fill kernel space page directory entry
  for(int i = 0; i < TOTAL_NUM_PDE; i++)
    pdt[i] = kernspace_pdt[i];

  return KERN_VMEM_TO_PHYS(pdt);
}

void pagetbl_add_mapping(u32 *pdt, vaddr_t vaddr, paddr_t paddr) {
  //printf("add_mapping: pid %d, vaddr %x, paddr %x\n", current->pid, vaddr, paddr);
  u32 *v_pdt = (u32 *)PHYS_TO_KERN_VMEM(pdt);
  int pdtindex = vaddr>>22;
  int ptindex = (vaddr>>12) & 0x3ff;
  if((v_pdt[pdtindex] & PDE_PRESENT) == 0) {
    v_pdt[pdtindex] = KERN_VMEM_TO_PHYS((u32)get_zeropage()) | PDE_PRESENT | PDE_RW | PDE_USER;
  }

  u32 *pt = (u32 *)(PHYS_TO_KERN_VMEM(v_pdt[pdtindex] & ~0xfff));
  pt[ptindex] = (paddr & ~0xfff) | PTE_PRESENT | PTE_RW | PTE_USER;
  flushtlb(pdt);
}

void pagetbl_free(paddr_t pdt) {
  u32 *v_pdt = (u32 *)PHYS_TO_KERN_VMEM(pdt);
  for(int i = 0; i < KERN_PDE_START; i++) {
    u32 ent = v_pdt[i];
    if((ent & PDE_PRESENT)) {
      page_free(PHYS_TO_KERN_VMEM(ent & ~0xfff));
    }
  }

  page_free(v_pdt);
}

static vaddr_t pagetbl_dup_one(vaddr_t oldpt) {
  //copy page table entry
  u32 *newpt = get_zeropage();

  for(int i=0; i<TOTAL_NUM_PTE; i++) {
    u32 oldent = ((u32 *)oldpt)[i];
    if(oldent & PTE_PRESENT) {
      newpt[i] = (oldent & ~0xfff) | PTE_PRESENT | PTE_USER; //Do not set PTE_RW due to copy-on-write.
      ((u32 *)oldpt)[i] &= ~PTE_RW; //for copy-on-write
    }
  }
  return (vaddr_t)newpt;
}

paddr_t pagetbl_dup_for_fork(paddr_t oldpdt) {
  u32 *pdt = page_alloc();
  u32 *v_oldpdt = (u32 *)PHYS_TO_KERN_VMEM(oldpdt);

  //fill kernel space page directory entry
  for(int i = 0; i < TOTAL_NUM_PDE; i++)
    pdt[i] = kernspace_pdt[i];

  //copy user space page directory entry
  for(int i = 0; i < KERN_PDE_START; i++) {
    u32 oldent = v_oldpdt[i];
    if(oldent & PDE_PRESENT) {
      vaddr_t newpt = pagetbl_dup_one(PHYS_TO_KERN_VMEM(oldent & ~0xfff));
      pdt[i] = KERN_VMEM_TO_PHYS(newpt) | PDE_PRESENT | PDE_RW | PDE_USER;
    }
  }

  flushtlb(oldpdt);

  return KERN_VMEM_TO_PHYS(pdt);
}

