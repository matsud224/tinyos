#pragma once
#include <kern/kernlib.h>

void pagetbl_init(void);
paddr_t pagetbl_new(void);
void pagetbl_free(paddr_t pdt);
void pagetbl_add_mapping(u32 *pdt, u32 vaddr, u32 paddr);
paddr_t pagetbl_dup_for_fork(paddr_t oldtbl);
