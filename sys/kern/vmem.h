#pragma once

#include <stdint.h>
#include <stddef.h>
#include <kern/fs.h>

struct mapper;
struct vm_map;

struct vm_map {
  struct vm_area *area_list;
  u32 flags; 
};

struct vm_area {
  struct vm_map *submap;
  vaddr_t start;
  off_t offset;
  size_t size;
  u32 flags;
  struct mapper *mapper;
  struct vm_area *next;
};

struct mapper {
  const struct mapper_ops *ops;
  struct vm_area *area;
};

struct vm_map *vm_map_new(void);
int vm_add_area(struct vm_map *map, vaddr_t start, size_t size, struct mapper *mapper, u32 flags);
struct vm_area *vm_findarea(struct vm_map *map, vaddr_t addr);
void vmem_init(void);

struct mapper *anon_mapper_new(void);
struct mapper *vnode_mapper_new(struct vnode *vnode, off_t file_off, size_t len);
