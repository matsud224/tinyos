#pragma once

#include "pagetbl.h"
#include <stdint.h>
#include <stddef.h>

extern uint32_t *current_pdt;

void pagetbl_init(void);
void pagetbl_add_mapping(uint32_t *pdt, uint32_t vaddr, uint32_t paddr);
