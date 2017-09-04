#include "vmem.h"
#include "params.h"
#include "page.h"
#include "malloc.h"
#include <stdint.h>
#include <stddef.h>

#define VM_AREA_HAVE_SUBMAP 0x1

struct vm_area;

struct vm_map {
  struct vm_area *area_list;
  uint32_t flags; 
};

struct vm_area {
  struct vm_map *submap;
  uint32_t start;
  size_t size;
  size_t offset; // offset of mapper-related object
  uint32_t flags;
  struct mapper *mapper;
  struct vm_area *next;
};

struct mapper_ops {
  uint32_t (*request)(void *info, uint32_t offset);
};

struct mapper {
  const struct mapper_ops *ops;
  void *info;
};

struct anon_mapper {
  size_t size;
};

uint32_t anon_mapper_request(void *info, uint32_t offset) {
  return (uint32_t)page_alloc();
}

static const struct mapper_ops anon_mapper_ops = {
  .request = anon_mapper_request
};

static struct mapper *anon_mapper_new(uint32_t size) {
  struct mapper *m;
  struct anon_mapper *anon;
  if((m = malloc(sizeof(struct mapper))) == NULL)
    return NULL;
  if((anon = malloc(sizeof(struct anon_mapper))) == NULL) {
    free(m);
    return NULL;
  }

  anon->size = size;
  m->ops = &anon_mapper_ops;
  m->info = anon;
  return m;
}

int vm_add_area(struct vm_map *map, uint32_t start, size_t size, struct mapper *mapper, uint32_t flags) {
  struct vm_area *a;

  start = start & ~(PAGESIZE-1);
  size = (size+(PAGESIZE-1)) & ~(PAGESIZE-1);

  //check overlap
  for(a=map->area_list; a!=NULL; a=a->next) {
    if(start < a->start && (start+size-1) >= a->start)
      return -1;
    else if(start >= a->start && start <= (a->start+a->size-1))
      return -1;
  }

  struct vm_area *new = malloc(sizeof(struct vm_area));
  if(new == NULL)
    return -1;
  new->submap = NULL;
  new->start = start;
  new->size = size;
  new->offset = 0;
  new->flags = 0;
  if((new->mapper = anon_mapper_new(size)) == NULL) {
    free(new);
    return -1;
  }
  new->next = map->area_list;
  map->area_list = new;

  return 0;
}

/*
int vm_remove_area(struct vm_map *map, uint32_t start, size_t size) {
  struct vm_area *a;

  start = start & ~(PAGESIZE-1);
  size = (size+(PAGESIZE-1)) & ~(PAGESIZE-1);

  for(a=map->area_list; a!=NULL; a=a->next) {
    if(start <= a->start )
      return -1;
    else if(start >= a->start && start <= (a->start+a->size-1))
      return -1;
  }
}
*/

struct vm_area *vm_findarea(struct vm_map *map, uint32_t addr) {
  struct vm_area *a;
  for(a=map->area_list; a!=NULL; a=a->next)
    if(a->start >= addr && (a->start+a->size) > addr) {
      if(a->flags & VM_AREA_HAVE_SUBMAP)
        return vm_findarea(a->submap, addr);
      else
        return a;
    }
  return NULL;
}


