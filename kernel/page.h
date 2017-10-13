#pragma once
#include "kernlib.h"

void page_init(void);
int page_getnfree(void);
void *page_alloc(void);
void page_free(u32 addr);
void bzero(void *s, size_t n);
void *get_zeropage();
