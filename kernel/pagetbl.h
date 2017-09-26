#pragma once

#include "pagetbl.h"
#include <stdint.h>
#include <stddef.h>

uint32_t *new_procpdt(void);
void pagetbl_init(void);
void pagetbl_add_mapping(uint32_t *pdt, uint32_t vaddr, uint32_t paddr);
