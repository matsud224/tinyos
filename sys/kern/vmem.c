#include <kern/kernlib.h>
#include <kern/vmem.h>
#include <kern/page.h>
#include <kern/fs.h>
#include <kern/file.h>

#define VM_AREA_HAVE_SUBMAP 0x1

struct anon_mapper {
  struct mapper mapper;
};

struct file_mapper {
  struct file *file;
  off_t file_off;
  size_t len;
  struct mapper mapper;
};

void *anon_mapper_request(struct mapper *m UNUSED, vaddr_t offset UNUSED) {
  void *p = get_zeropage();
  return p;
}

static const struct mapper_ops anon_mapper_ops = {
  .request = anon_mapper_request
};

struct mapper *anon_mapper_new() {
  struct anon_mapper *m;
  if((m = malloc(sizeof(struct anon_mapper))) == NULL)
    return NULL;

  m->mapper.ops = &anon_mapper_ops;
  return &(m->mapper);
}

void *file_mapper_request(struct mapper *m, vaddr_t in_area_off) {
  void *p = get_zeropage();
  struct file_mapper *fm = container_of(m, struct file_mapper, mapper);
  vaddr_t st = pagealign(in_area_off);
  vaddr_t end = pagealign(in_area_off) + PAGESIZE;
  size_t readlen = PAGESIZE;
  if(st <= m->area->offset + fm->len && end > m->area->offset + fm->len)
    readlen -= end - (m->area->offset + fm->len);
  if(st <= m->area->offset && end > m->area->offset)
    readlen -= m->area->offset;

  if(readlen != 0) {
    int read_bytes;
    lseek(fm->file, st + fm->file_off, SEEK_SET);
    if(st <= m->area->offset && end > m->area->offset)
      read_bytes = read(fm->file, p+m->area->offset, readlen);
    else
      read_bytes = read(fm->file, p, readlen);

    //printf("read: %x st: %x,areaoff: %x file_off: %x, len:%x, fileoff: %x\n", read_bytes, st, (u32)m->area->offset, (u32)fm->file_off, (u32)fm->len, (u32)(st - m->area->offset + fm->file_off));
  }
  return p;
}

static const struct mapper_ops file_mapper_ops = {
  .request = file_mapper_request
};

struct mapper *file_mapper_new(struct file *file, off_t file_off, size_t len) {
  struct file_mapper *m;
  if((m = malloc(sizeof(struct file_mapper))) == NULL)
    return NULL;

  m->file = file;
  m->file_off = file_off;
  m->len = len;
  m->mapper.ops = &file_mapper_ops;
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

void vm_map_free(struct vm_map *vmmap) {

}

void vm_show_area(struct vm_map *map) {
  struct vm_area *a;

  for(a=map->area_list; a!=NULL; a=a->next) {
    printf("vm_add_area: from %x size %x offset %x\n", a->start, a->size, a->offset);
  }
}


int vm_add_area(struct vm_map *map, u32 start, size_t size, struct mapper *mapper, u32 flags UNUSED) {
  struct vm_area *a;

  size += start & (PAGESIZE-1);
  size = (size+(PAGESIZE-1)) & ~(PAGESIZE-1);
  size_t offset = start & (PAGESIZE-1);
  start = start & ~(PAGESIZE-1);

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
  new->offset = offset;
  new->flags = 0;
  new->mapper = mapper;
  new->next = map->area_list;
  map->area_list = new;
  mapper->area = new;
//printf("vm_add_area: from %x size %x(%x) offset %x\n", new->start, new->size, size, new->offset);
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

struct vm_area *vm_findarea(struct vm_map *map, vaddr_t addr) {
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
