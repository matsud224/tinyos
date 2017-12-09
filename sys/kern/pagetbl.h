#pragma once
#include <kern/kernlib.h>

u32 *procpdt_new(void);
void pagetbl_init(void);
void pagetbl_add_mapping(u32 *pdt, u32 vaddr, u32 paddr);
