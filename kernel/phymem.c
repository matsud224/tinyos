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

static int freepages;
 
int phymem_getfreepages() {
  return freepages;
}

void phymem_init() {
  struct ardesc *map = (struct ardesc *)MEMORYMAP_ADDR;
  freepages = 0;
  while(map->base || map->len) {
    if(map->type == 1) {
printf("%x - %x\n", (uint32_t)map->base, (uint32_t)(map->base + map->len));
      freepages += (map->len - ((ALIGN(map->base, PAGESIZE) - map->base))) / PAGESIZE;
    }
    map++;
  }
  return;
}
