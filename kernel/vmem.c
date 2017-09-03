#include "vmem.h"
#include <stdint.h>
#include <stddef.h>

#define VM_AREA_HAVE_SUBMAP

struct vm_area;

struct vm_map {
  struct vm_area *area_list;
  uint32_t flags; 
};

struct vm_area {
  struct vm_map *submap;
  uint32_t start;
  uint32_t size;
  uint32_t flags;
  struct vm_area *next;
};



int vm_add_area(struct vm_map *map, uint32_t start, uint32_t size, uint32_t flags) {
  struct vm_area *a;

  for(a=map->area_list; a!=NULL; a=a->next) {
    
  }

  return -1;
}
