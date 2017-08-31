#include "phymem.h"
#include <stdint.h>
#include "params.h"
#include "common.h"

// address region descriptor
struct ardesc {
  uint64_t base;
  uint64_t len;
  uint32_t type;
};

static uint32_t bitmap_addr = PHYMEM_MANAGE_ADDR; // free bitmap area 
static uint32_t begin_addr;
static uint32_t memsize;
static uint32_t freepages;

 
int phymem_getfreepages() {
  return freepages;
}

void phymem_init() {
  struct ardesc *map = (struct ardesc *)MEMORYMAP_ADDR;
  while(map->base || map->len) {
    if(map->type == 1)
      memsize = (uint32_t)(map->base + map->len);
    map++;
  }
  memsize = memsize / PAGESIZE * PAGESIZE;

  freepages = (map->len - ((ALIGN(map->base, PAGESIZE) - map->base))) / PAGESIZE;
  return;
}
