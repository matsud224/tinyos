#pragma once
#include <kern/kernlib.h>
#include <kern/multiboot.h>

void page_init(struct multiboot_info *);
int page_getnfree(void);
void *page_alloc(size_t, int);
void page_free(void *addr);
void bzero(void *s, size_t n);
void *get_zeropage(size_t);
