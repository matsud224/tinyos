#include "page.h"
#include <stdint.h>
#include "params.h"
#include "common.h"
#include <stddef.h>
#include "vga.h" 

// address region descriptor
struct ardesc {
  uint64_t base;
  uint64_t len;
  uint32_t type;
};


#define PAGE_RESERVED 0x2
#define PAGE_ALLOCATED 0x1

struct page {
  struct page *next_free;
  uint32_t flags;
};

static size_t memsize;
struct page *pageinfo = NULL;
struct page *page_freelist = NULL;
static uint32_t page_nfree;
static uint32_t protmem_freearea_addr;

int page_getnfree() {
  return page_nfree;
}

void *page_alloc() {
  if(page_freelist == NULL)
    return NULL;
  page_nfree--;
  struct page *new = page_freelist;
  page_freelist = page_freelist->next_free;
  new->next_free = NULL;
  new->flags |= PAGE_ALLOCATED;
  uint32_t phyaddr = (size_t)((uint32_t)new-(KERNSPACE_ADDR+PROTMEM_ADDR))/sizeof(struct page) * PAGESIZE;
  return (void *)(phyaddr + KERNSPACE_ADDR);
}

void page_free(uint32_t addr) {
  addr -= KERNSPACE_ADDR;
  int index = addr / PAGESIZE;
  pageinfo[index].flags &= ~PAGE_ALLOCATED;
  pageinfo[index].next_free = page_freelist;
  page_freelist = &pageinfo[index];
  page_nfree++;
}

static void recycle_area(uint32_t start, size_t size) {
  int startindex = (start+(PAGESIZE-1)) / PAGESIZE;
  int endindex = startindex + size / PAGESIZE;
printf("%d - %d\n", startindex, endindex); 
  for(int i=startindex; i<endindex; i++) {
    pageinfo[i].flags &= ~PAGE_RESERVED;
    pageinfo[i].next_free = page_freelist;
    page_freelist = &pageinfo[i];
    page_nfree++;
  }
}

void page_init() {
  struct ardesc *map = (struct ardesc *)(KERNSPACE_ADDR+MEMORYMAP_ADDR);
  while(map->base || map->len) {
    if(map->type == 1)
      memsize = (uint32_t)(map->base + map->len);
    map++;
  }
  memsize = MIN(memsize, KERN_STRAIGHT_MAP_SIZE);
  memsize = memsize & ~(PAGESIZE-1);

  pageinfo = (struct page *)(KERNSPACE_ADDR+PROTMEM_ADDR);
  int npages = memsize / PAGESIZE;
printf("npages =  %d\n", npages); 
  for(int i=0; i<npages; i++)
    pageinfo[i].flags = PAGE_RESERVED;

  protmem_freearea_addr = (uint32_t)&pageinfo[npages];
  page_nfree = 0;
  page_freelist = NULL;
  recycle_area(protmem_freearea_addr-KERNSPACE_ADDR, memsize - (((uint32_t)protmem_freearea_addr-KERNSPACE_ADDR)));
  
  return;
}


