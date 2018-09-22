#include <kern/kernlib.h>
#include <kern/vmem.h>
#include <kern/page.h>
#include <kern/fs.h>
#include <kern/file.h>


struct page_entry {
  struct list_head link;
  void *addr;
};

struct anon_mapper {
  struct mapper mapper;
  struct list_head page_list;
};

struct file_mapper {
  struct file *file;
  off_t file_off;
  size_t len;
  struct mapper mapper;
};

void *anon_mapper_request(struct mapper *m, vaddr_t offset UNUSED) {
  struct anon_mapper *am = container_of(m, struct anon_mapper, mapper);
  struct page_entry *pe;
  if((pe = malloc(sizeof(struct page_entry))) == NULL)
    return NULL;
  pe->addr = get_zeropage();
  list_pushback(&pe->link, &am->page_list);
  return pe->addr;
}

int anon_mapper_yield(struct mapper *m) {
  //TODO: swapping
  return -1;
}

static page_entry_free(struct page_entry *pe) {
  page_free(pe->addr);
  free(pe);
}

void anon_mapper_remove(struct mapper *m) {
  struct anon_mapper *am = container_of(m, struct file_mapper, mapper);
  list_free_all(&am->page_list, struct page_entry, link, page_entry_free);
}

static const struct mapper_ops anon_mapper_ops = {
  .request = anon_mapper_request,
  .yield = anon_mapper_yield,
  .remove = anon_mapper_remove,
};

struct mapper *anon_mapper_new() {
  struct anon_mapper *am;
  if((am = malloc(sizeof(struct anon_mapper))) == NULL)
    return NULL;
  list_init(&am->page_list);

  am->mapper.ops = &anon_mapper_ops;
  return &(am->mapper);
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

    if(read_bytes < readlen)
      puts("fatal: read failed");
  }
  return p;
}

int file_mapper_yield(struct mapper *m) {
  //TODO: swapping
  return -1;
}

void file_mapper_remove(struct mapper *m) {
  struct file_mapper *fm = container_of(m, struct file_mapper, mapper);
}


static const struct mapper_ops file_mapper_ops = {
  .request = file_mapper_request,
  .yield = file_mapper_yield,
  .remove = file_mapper_remove,
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

  list_init(&m->area_list);
  m->flags = 0;
  return m;
}

void vm_map_free(struct vm_map *vmmap) {

}

int vm_add_area(struct vm_map *map, u32 start, size_t size, struct mapper *mapper, u32 flags UNUSED) {
  struct list_head *p;

  size += start & (PAGESIZE-1);
  size = (size+(PAGESIZE-1)) & ~(PAGESIZE-1);
  size_t offset = start & (PAGESIZE-1);
  start = start & ~(PAGESIZE-1);

  //check overlap
  list_foreach(p, &map->area_list) {
    struct vm_area *a = list_entry(p, struct vm_area, link);
    if(start < a->start && (start+size-1) >= a->start)
      return -1;
    else if(start >= a->start && start <= (a->start+a->size-1))
      return -1;
  }

  struct vm_area *new = malloc(sizeof(struct vm_area));
  if(new == NULL)
    return -1;
  new->start = start;
  new->size = size;
  new->offset = offset;
  new->flags = 0;
  new->mapper = mapper;
  list_pushback(&new->link, &map->area_list);
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
  struct list_head *p;
  list_foreach(p, &map->area_list) {
    struct vm_area *a = list_entry(p, struct vm_area, link);
    //printf("area 0x%x - 0x%x\n", a->start, a->start+a->size);
    if(a->start <= addr && (a->start+a->size) > addr) {
      return a;
    }
  }
  return NULL;
}

void vmem_init() {
}
