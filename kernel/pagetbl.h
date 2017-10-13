#pragma once
#include "kernlib.h"

u32 *new_procpdt(void);
void pagetbl_init(void);
void pagetbl_add_mapping(u32 *pdt, u32 vaddr, u32 paddr);
