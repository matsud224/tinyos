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
  u32 start;
  size_t size;
  size_t offset; // offset of mapper-related object
  u32 flags;
  struct mapper *mapper;
  struct vm_area *next;
};

struct mapper_ops {
  u32 (*request)(struct mapper *m, u32 offset);
};

struct mapper {
  const struct mapper_ops *ops;
};

struct vm_map *vm_map_new(void);
int vm_add_area(struct vm_map *map, u32 start, size_t size, struct mapper *mapper, u32 flags);
struct vm_area *vm_findarea(struct vm_map *map, u32 addr);
void vmem_init(void);

struct mapper *anon_mapper_new(u32 size);
struct mapper *inode_mapper_new(struct inode *inode, u32 offset);
