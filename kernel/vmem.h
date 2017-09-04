#pragma once

#include <stdint.h>
#include <stddef.h>

struct mapper;
struct vm_map;

int vm_add_area(struct vm_map *map, uint32_t start, size_t size, struct mapper *mapper, uint32_t flags);

