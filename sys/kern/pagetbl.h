#pragma once
#include <kern/kernlib.h>

paddr_t procpdt_new(void);
void pagetbl_init(void);
void pagetbl_add_mapping(u32 *pdt, u32 vaddr, u32 paddr);
paddr_t pagetbl_dup_for_fork(paddr_t oldtbl);
