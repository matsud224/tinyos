#include <kern/page.h>
#include <kern/kernlib.h>

// address region descriptor
struct ardesc {
  u64 base;
  u64 len;
  u32 type;
};

#define PAGE_RESERVED 0x2
#define PAGE_ALLOCATED 0x1

struct page {
  struct page *next_free;
  u32 flags;
};

static size_t memsize;
struct page *pageinfo = NULL;
struct page *page_freelist = NULL;
static u32 page_nfree;
static u32 protmem_freearea_addr;

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
  paddr_t paddr = (size_t)((vaddr_t)new-(KERN_VMEM_ADDR+PROTMEM_ADDR))/sizeof(struct page) * PAGESIZE;
  return (void *)PHYS_TO_KERN_VMEM(paddr);
}

void page_free(void *addr) {
  paddr_t paddr = KERN_VMEM_TO_PHYS(addr);
  int index = paddr / PAGESIZE;
  pageinfo[index].flags &= ~PAGE_ALLOCATED;
  pageinfo[index].next_free = page_freelist;
  page_freelist = &pageinfo[index];
  page_nfree++;
}

static void recycle_area(paddr_t start, size_t size) {
  int startindex = (start+(PAGESIZE-1)) / PAGESIZE;
  int endindex = startindex + size / PAGESIZE;
  for(int i=startindex; i<endindex; i++) {
    pageinfo[i].flags &= ~PAGE_RESERVED;
    pageinfo[i].next_free = page_freelist;
    page_freelist = &pageinfo[i];
    page_nfree++;
  }
}

void page_init() {
  struct ardesc *map = (struct ardesc *)(KERN_VMEM_ADDR+MEMORYMAP_ADDR);
  while(map->base || map->len) {
    if(map->type == 1)
      memsize = (u32)(map->base + map->len);
    map++;
  }
  memsize = MIN(memsize, KERN_STRAIGHT_MAP_SIZE);
  memsize = memsize & ~(PAGESIZE-1);

  pageinfo = (struct page *)(KERN_VMEM_ADDR+PROTMEM_ADDR);
  int npages = memsize / PAGESIZE;
  for(int i=0; i<npages; i++)
    pageinfo[i].flags = PAGE_RESERVED;

  protmem_freearea_addr = (u32)&pageinfo[npages];
  page_nfree = 0;
  page_freelist = NULL;
  recycle_area(KERN_VMEM_TO_PHYS(protmem_freearea_addr), memsize - (((u32)KERN_VMEM_TO_PHYS(protmem_freearea_addr))));

  printf("page: %d MB(%d pages) free\n", (page_getnfree()*4)/1024, page_getnfree());
  return;
}

void *get_zeropage() {
  void *p = page_alloc();
  if(p)
    bzero(p, PAGESIZE);
  return p;
}
