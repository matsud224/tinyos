#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kern/fs.h>
#include <kern/kernlib.h>

struct mapper;
struct vm_map;

struct vm_map {
  struct list_head area_list;
  u32 flags;
};

struct vm_area {
  struct list_head link;
  vaddr_t start;
  off_t offset;
  size_t size;
  u32 flags;
  struct mapper *mapper;
};

struct mapper_ops {
  paddr_t (*request)(struct mapper *m, vaddr_t offset);
  int (*yield)(struct mapper *m, paddr_t pdt);
  void (*free)(struct mapper *m);
  struct mapper *(*dup)(struct mapper *m);
};

struct mapper {
  const struct mapper_ops *ops;
  struct vm_area *area;
  struct list_head page_list;
};

struct vm_map *vm_map_new(void);
void vm_map_free(struct vm_map *vmmap);
int vm_map_yield(struct vm_map *vmmap, paddr_t pdt);
struct vm_map *vm_map_dup(struct vm_map *oldm);
int vm_add_area(struct vm_map *map, vaddr_t start, size_t size, struct mapper *mapper, u32 flags);
struct vm_area *vm_findarea(struct vm_map *map, vaddr_t addr);
void vm_show_area(struct vm_map *map);
void vmem_init(void);

struct mapper *anon_mapper_new(void);
vaddr_t anon_mapper_add_page(struct mapper *m, vaddr_t start);
struct mapper *file_mapper_new(struct file *file, off_t file_off, size_t len);
