/*
#include <kern/kernlib.h>
#include <kern/addrspace.h>

struct addrspace *addrspace_new(const struct addrspace_ops *ops) {
  struct addrspace *as = malloc(sizeof(struct addrspace));
  bzero(as, sizeof(struct addrspace));

  list_init(&as->pagelist);

  return as;
}

void *addrspace_find(struct addrspace *as, vaddr_t addr) {
  addr &= ~(PAGESIZE-1);
  struct list_head *p;
  list_foreach(p, &as->pagelist) {
    struct pageent *ent = list_entry(p, struct pageent, link);
    if(ent->addr == addr)
      return ent->page;
  }

  return NULL;
}

void addrspace_add(struct addrspace *as, vaddr_t addr, void *page) {
  addr &= ~(PAGESIZE-1);
  struct pageent *ent = malloc(sizeof(struct pageent));
  ent->addr = addr;
  ent->page = page;

  list_pushback(&ent->link, &as->pagelist);
}

void addrspace_remove(struct addrspace *as, vaddr_t addr) {
  addr &= ~(PAGESIZE-1);
  struct list_head *p, *tmp;
  list_foreach_safe(p, tmp, &as->pagelist) {
    struct pageent *ent = list_entry(p, struct pageent, link);
    if(ent->addr == addr) {
      list_remove(&ent->link);
      return;
    }
  }
}


*/
