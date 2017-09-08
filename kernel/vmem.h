#pragma once

#include <stdint.h>
#include <stddef.h>

struct mapper;
struct vm_map;

struct vm_map {
  struct vm_area *area_list;
  uint32_t flags; 
};

struct vm_area {
  struct vm_map *submap;
  uint32_t start;
  size_t size;
  size_t offset; // offset of mapper-related object
  uint32_t flags;
  struct mapper *mapper;
  struct vm_area *next;
};

struct mapper_ops {
  uint32_t (*request)(void *info, uint32_t offset);
};

struct mapper {
  const struct mapper_ops *ops;
  void *info;
};

extern struct vm_map *current_vmmap;

struct vm_map *vm_map_new(void);
int vm_add_area(struct vm_map *map, uint32_t start, size_t size, struct mapper *mapper, uint32_t flags);
struct vm_area *vm_findarea(struct vm_map *map, uint32_t addr);
void vmem_init(void);