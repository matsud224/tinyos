struct page_head {
  struct list_head link;
  u32 addr;
  void *page;
}

struct address_space {
  struct list_head page_list; 
  
};

struct address_space *address_space_new() {
  struct address_space *as = malloc(sizeof(struct address_space));
  list_init(&as->page_list);
  return as;
}

struct 
