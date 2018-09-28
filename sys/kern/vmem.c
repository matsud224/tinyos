#include <kern/kernlib.h>
#include <kern/vmem.h>
#include <kern/page.h>
#include <kern/fs.h>
#include <kern/file.h>
#include <kern/thread.h>

struct page_entry {
  struct list_head link;
  struct page_info *pinfo;
};

struct page_info {
  void *addr;
  int ref;
  vaddr_t start;
  mutex mtx;
};

struct anon_mapper {
  struct mapper mapper;
};

struct file_mapper {
  struct mapper mapper;
  struct file *file;
  off_t file_off;
  size_t len;
};

struct page_entry *page_entry_new(vaddr_t start) {
  void *p = get_zeropage();
  if(p == NULL)
    return NULL;

  struct page_entry *pe = malloc(sizeof(struct page_entry));
  struct page_info *pi = malloc(sizeof(struct page_info));
  pi->addr = p;
  pi->ref = 1;
  pi->start = start;
  mutex_init(&pi->mtx);
  pe->pinfo = pi;
  return pe;
}

struct page_entry *page_entry_find(struct list_head *page_list, vaddr_t start) {
  struct list_head *p;
  list_foreach(p, page_list) {
    struct page_entry *pe = list_entry(p, struct page_entry, link);
    if(pe->pinfo->start == start)
      return pe;
  }
  return NULL;
}

static void page_copy(struct page_entry *pe) {
  if(pe->pinfo->ref == 1) {
    return;
  }

  struct page_info *pinew = malloc(sizeof(struct page_info));
  memcpy(pinew, pe->pinfo, sizeof(struct page_info));
  pinew->addr = page_alloc();
  memcpy(pinew->addr, pe->pinfo->addr, PAGESIZE);
  pinew->ref = 1;
  mutex_init(&pinew->mtx);
  pe->pinfo->ref--;
  pe->pinfo = pinew;
}

paddr_t anon_mapper_request(struct mapper *m, vaddr_t offset) {
  struct anon_mapper *am = container_of(m, struct anon_mapper, mapper);
  vaddr_t start = pagealign(m->area->start+offset);
  vaddr_t page = anon_mapper_add_page(m, start);
  return KERN_VMEM_TO_PHYS(page);
}

int anon_mapper_yield(struct mapper *m UNUSED, paddr_t pdt UNUSED) {
  //TODO: swapping
  return -1;
}

vaddr_t anon_mapper_add_page(struct mapper *m, vaddr_t start) {
  struct anon_mapper *am = container_of(m, struct anon_mapper, mapper);
  struct page_entry *pe;
  if((pe = page_entry_find(&m->page_list, start))) {
    page_copy(pe);
  } else {
    pe = page_entry_new(start);
    if(pe == NULL)
      return NULL;
    list_pushback(&pe->link, &m->page_list);
  }

  return pe->pinfo->addr;
}

static void page_entry_free(struct page_entry *pe) {
  if(--(pe->pinfo->ref) == 0) {
    page_free(pe->pinfo->addr);
    free(pe->pinfo);
  }
  free(pe);
}

void anon_mapper_free(struct mapper *m) {
  struct anon_mapper *am = container_of(m, struct anon_mapper, mapper);
  list_free_all(&m->page_list, struct page_entry, link, page_entry_free);
  free(am);
}

static void page_list_dup(struct list_head *src, struct list_head *dest) {
  struct list_head *p;
  list_foreach(p, src) {
    struct page_entry *pe = list_entry(p, struct page_entry, link);
    pe->pinfo->ref++;
    struct page_entry *pe2 = malloc(sizeof(struct page_entry));
    pe2->pinfo = pe->pinfo;
    list_pushback(&pe2->link, dest);
  }
}

struct mapper *anon_mapper_dup(struct mapper *m) {
  struct anon_mapper *amold = container_of(m, struct anon_mapper, mapper);
  struct anon_mapper *amnew = malloc(sizeof(struct anon_mapper));
  memcpy(amnew, amold, sizeof(struct anon_mapper));

  list_init(&amnew->mapper.page_list);

  page_list_dup(&amold->mapper.page_list, &amnew->mapper.page_list);
  return &amnew->mapper;
}


static const struct mapper_ops anon_mapper_ops = {
  .request = anon_mapper_request,
  .yield = anon_mapper_yield,
  .free = anon_mapper_free,
  .dup = anon_mapper_dup,
};

struct mapper *anon_mapper_new() {
  struct anon_mapper *am;
  if((am = malloc(sizeof(struct anon_mapper))) == NULL)
    return NULL;
  list_init(&am->mapper.page_list);

  am->mapper.ops = &anon_mapper_ops;
  return &(am->mapper);
}

paddr_t file_mapper_request(struct mapper *m, vaddr_t in_area_off) {
  struct file_mapper *fm = container_of(m, struct file_mapper, mapper);
  vaddr_t st = pagealign(in_area_off);
  vaddr_t end = pagealign(in_area_off) + PAGESIZE;

  vaddr_t start = pagealign(m->area->start+in_area_off);
  struct page_entry *pe;
  if((pe = page_entry_find(&m->page_list, start))) {
    //this page already exists but requested ... copy-on-write
    page_copy(pe);
  } else {
    pe = page_entry_new(start);
    if(pe == NULL)
      return NULL;
    list_pushback(&pe->link, &m->page_list);

    size_t readlen = PAGESIZE;
    if(st <= m->area->offset + fm->len && end > m->area->offset + fm->len)
      readlen -= end - (m->area->offset + fm->len);
    if(st <= m->area->offset && end > m->area->offset)
      readlen -= m->area->offset;

    if(readlen != 0) {
      u32 read_bytes;
      mutex_lock(&pe->pinfo->mtx);
      lseek(fm->file, st + fm->file_off, SEEK_SET);
      if(st <= m->area->offset && end > m->area->offset)
        read_bytes = read(fm->file, pe->pinfo->addr+m->area->offset, readlen);
      else
        read_bytes = read(fm->file, pe->pinfo->addr, readlen);

      if(read_bytes < readlen)
        puts("fatal: read failed");

      mutex_unlock(&pe->pinfo->mtx);
    }
  }

  return KERN_VMEM_TO_PHYS(pe->pinfo->addr);
}

int file_mapper_yield(struct mapper *m, paddr_t pdt) {
  //TODO: pages may be dirty!
  return -1;
  /*
  struct file_mapper *fm = container_of(m, struct file_mapper, mapper);

  int count = 0;
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &m->page_list) {
    struct page_entry *pe = list_entry(p, struct page_entry, link);
    if(mutex_trylock(&pe->pinfo->mtx) == 0) {
      if(pe->pinfo->ref == 1) {
        pagetbl_remove_mapping(pdt, pe->pinfo->start);
        list_remove(&pe->link);
        page_entry_free(pe);
        count++;
      } else {
        mutex_unlock(&pe->pinfo->mtx);
      }
    }
  }
  return (count > 0)?0:-1;
  */
}

void file_mapper_free(struct mapper *m) {
  struct file_mapper *fm = container_of(m, struct file_mapper, mapper);
  list_free_all(&m->page_list, struct page_entry, link, page_entry_free);
  close(fm->file);
  free(fm);
}

struct mapper *file_mapper_dup(struct mapper *m) {
  struct file_mapper *fmold = container_of(m, struct file_mapper, mapper);
  struct file_mapper *fmnew = malloc(sizeof(struct file_mapper));
  memcpy(fmnew, fmold, sizeof(struct file_mapper));

  fmnew->file = dup(fmold->file);
  list_init(&fmnew->mapper.page_list);
  page_list_dup(&fmold->mapper.page_list, &fmnew->mapper.page_list);
  return &fmnew->mapper;
}

static const struct mapper_ops file_mapper_ops = {
  .request = file_mapper_request,
  .yield = file_mapper_yield,
  .free = file_mapper_free,
  .dup = file_mapper_dup,
};

struct mapper *file_mapper_new(struct file *file, off_t file_off, size_t len) {
  struct file_mapper *fm;
  if((fm = malloc(sizeof(struct file_mapper))) == NULL)
    return NULL;

  fm->file = file;
  fm->file_off = file_off;
  fm->len = len;
  fm->mapper.ops = &file_mapper_ops;
  list_init(&fm->mapper.page_list);
  return &(fm->mapper);
}


struct vm_map *vm_map_new() {
  struct vm_map *m;
  if((m = malloc(sizeof(struct vm_map))) == NULL)
    return NULL;

  list_init(&m->area_list);
  m->flags = 0;
  return m;
}


struct vm_area *vm_area_dup(struct vm_area *olda);

struct vm_map *vm_map_dup(struct vm_map *oldm) {
  struct vm_map *newm;
  if((newm = malloc(sizeof(struct vm_map))) == NULL)
    return NULL;

  list_init(&newm->area_list);
  newm->flags = oldm->flags;

  struct list_head *p;
  list_foreach(p, &oldm->area_list) {
    struct vm_area *a = list_entry(p, struct vm_area, link);
    struct vm_area *newa = vm_area_dup(a);
    list_pushback(&newa->link, &newm->area_list);
  }

  return newm;
}

struct vm_area *vm_area_dup(struct vm_area *olda) {
  struct vm_area  *newa = malloc(sizeof(struct vm_area));
  memcpy(newa, olda, sizeof(struct vm_area));
  newa->mapper = olda->mapper->ops->dup(olda->mapper);
  newa->mapper->area = newa;
  return newa;
}

void vm_area_free(struct vm_area *area) {
  area->mapper->ops->free(area->mapper);
  free(area);
}

void vm_map_free(struct vm_map *vmmap) {
  list_free_all(&vmmap->area_list, struct vm_area, link, vm_area_free);
  free(vmmap);
}

int vm_map_yield(struct vm_map *vmmap, paddr_t pdt) {
  struct list_head *p;
  list_foreach(p, &vmmap->area_list) {
    struct vm_area *area = list_entry(p, struct vm_area, link);
    if(area->mapper->ops->yield(area->mapper, pdt) == 0)
      return 0;
  }
  return -1;
}

int vm_add_area(struct vm_map *map, u32 start, size_t size, struct mapper *mapper, u32 flags UNUSED) {
  struct list_head *p;

  size += start & (PAGESIZE-1);
  size = pagealign(size+(PAGESIZE-1));
  size_t offset = start & (PAGESIZE-1);
  start = pagealign(start);

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

struct vm_area *vm_findarea(struct vm_map *map, vaddr_t addr) {
  struct list_head *p;
  list_foreach(p, &map->area_list) {
    struct vm_area *a = list_entry(p, struct vm_area, link);
    if(a->start <= addr && (a->start+a->size) > addr)
      return a;
  }
  puts("not found");
  return NULL;
}

void vm_show_area(struct vm_map *map) {
  struct list_head *p, *p2;
  puts("----- ----- -----");
  list_foreach(p, &map->area_list) {
    struct vm_area *a = list_entry(p, struct vm_area, link);;
    printf("  from %x size %x offset %x\n", a->start, a->size, a->offset);
    list_foreach(p2, &(a->mapper->page_list)) {
      struct page_entry *pe = list_entry(p2, struct page_entry, link);
      struct page_info *i = pe->pinfo;
      printf("    addr %x ref %x start %x\n", i->addr, i->ref, i->start);
    }
  }
  puts("----- ----- -----");
}

void vmem_init() {
}
