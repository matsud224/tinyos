#pragma once
#include <kern/kernlib.h>

void pagetbl_init(void);
paddr_t pagetbl_new(void);
void pagetbl_free(paddr_t pdt);
void pagetbl_add_mapping(u32 *pdt, vaddr_t vaddr, paddr_t paddr);
void pagetbl_remove_mapping(u32 *pdt, vaddr_t vaddr);
paddr_t pagetbl_dup_for_fork(paddr_t oldtbl);
