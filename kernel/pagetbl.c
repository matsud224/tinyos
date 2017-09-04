#include <stdint.h>
#include <stddef.h>
#include "kernasm.h"
#include "pagetbl.h"
#include "params.h"
#include "vga.h"
#include "page.h"

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


uint32_t *current_pdt;
static uint32_t *kernspace_pdt; //over 0xc0000000


static void bzero(void *s, size_t n) {
  uint8_t *ptr = s;
  while(n--)
    *ptr++ = 0;
}

static uint32_t *new_procpdt() {
  uint32_t *pdt = page_alloc();
  bzero(pdt, PAGESIZE);
  //fill kernel space page diectory entry
  int st_start_index = KERNSPACE_ADDR / 0x400000;
  for(int i = 0/*st_start_index*/; i < 1024; i++)
    pdt[i] = kernspace_pdt[i];
  return pdt;
}

void pagetbl_init() {
  //setup kernel space
  kernspace_pdt = page_alloc();
  bzero(kernspace_pdt, PAGESIZE);
  //kernel space straight mapping(896MB)
  int st_start_index = KERNSPACE_ADDR / 0x400000;
  int st_end_index = st_start_index + (KERN_STRAIGHT_MAP_SIZE/0x400000);
  uint32_t addr = 0x0; 
  for(int i = st_start_index; i < st_end_index;
        i++, addr += 0x400000){
    kernspace_pdt[i] = addr | PDE_PRESENT | PDE_RW | PDE_SIZE_4MB;
  }
  //kernel space virtual area
  for(int i = st_end_index; i < 1024; i++) {
    uint32_t *pt = page_alloc();
    bzero(pt, PAGESIZE);
    kernspace_pdt[i] = ((uint32_t)pt-KERNSPACE_ADDR) | PDE_PRESENT | PDE_RW;
  }
  //for process 0
  current_pdt = new_procpdt();

  flushtlb();
}


