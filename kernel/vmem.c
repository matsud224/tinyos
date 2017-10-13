#include "kernlib.h"
#include "vmem.h"
#include "page.h"
#include "fs.h"

#define VM_AREA_HAVE_SUBMAP 0x1

struct anon_mapper {
  size_t size;
  struct mapper mapper;
};

struct inode_mapper {
  struct inode *inode;
  size_t offset;
  struct mapper mapper;
};

u32 anon_mapper_request(struct mapper *m UNUSED, u32 offset UNUSED) {
  return (u32)page_alloc();
}

static const struct mapper_ops anon_mapper_ops = {
  .request = anon_mapper_request
};

struct mapper *anon_mapper_new(u32 size) {
  struct anon_mapper *m;
  if((m = malloc(sizeof(struct anon_mapper))) == NULL)
    return NULL;

  m->size = size;
  m->mapper.ops = &anon_mapper_ops;
  return &(m->mapper);
}

u32 inode_mapper_request(struct mapper *m, u32 offset) {
  u8 *p = page_alloc();
  struct inode_mapper *im = container_of(m, struct inode_mapper, mapper);
  fs_read(im->inode, p, im->offset + (offset & ~(PAGESIZE-1)), PAGESIZE);
  return (u32)p;
}

static const struct mapper_ops inode_mapper_ops = {
  .request = inode_mapper_request
};

struct mapper *inode_mapper_new(struct inode *inode, u32 offset) {
  struct inode_mapper *m;
  if((m = malloc(sizeof(struct inode_mapper))) == NULL)
    return NULL;

  m->inode = inode;
  m->offset = offset;
  m->mapper.ops = &inode_mapper_ops;
  return &(m->mapper);
}


struct vm_map *vm_map_new() {
  struct vm_map *m;
  if((m = malloc(sizeof(struct vm_map))) == NULL)
    return NULL;

  m->area_list = NULL;
  m->flags = 0;
  return m;
}

int vm_add_area(struct vm_map *map, u32 start, size_t size, struct mapper *mapper, u32 flags UNUSED) {
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
  new->mapper = mapper;
  new->next = map->area_list;
  map->area_list = new;
  return 0;
}

/*
int vm_remove_area(struct vm_map *map, u32 start, size_t size) {
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

struct vm_area *vm_findarea(struct vm_map *map, u32 addr) {
  struct vm_area *a;
  for(a=map->area_list; a!=NULL; a=a->next) {
    //printf("area 0x%x - 0x%x\n", a->start, a->start+a->size);
    if(a->start <= addr && (a->start+a->size) > addr) {
      if(a->flags & VM_AREA_HAVE_SUBMAP)
        return vm_findarea(a->submap, addr);
      else
        return a;
    }
  }
  return NULL;
}

void vmem_init() {
}
