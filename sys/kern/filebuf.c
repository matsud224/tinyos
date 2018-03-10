/*struct page_head {
  struct list_head link;
  u32 addr;
  void *page;
}

struct address_space {
  struct list_head page_list; 
  struct blkbuf * 
};

struct address_space *address_space_new() {
  struct address_space *as = malloc(sizeof(struct address_space));
  list_init(&as->page_list);
  return as;
}

void *address_space_findpage(struct address_space *as, u32 addr) {
  struct list_head *p;
  u32 page_addr = pagealign(addr);
  list_foreach(p, &as->page_list) {
    struct page_head *ph = list_entry(p, struct page_head, link);
    if(page_addr == ph->addr)
      return ph->addr;
  }

  return NULL;
}

void address_space_addpage(struct address_space *as, u32 addr, void *page) {
  struct page_head *ph = malloc(sizeof(struct page_head));
  ph->addr = pagealign(addr);
  ph->page = page;
  list_pushback(&ph->link, &as->page_list);
}

*/
