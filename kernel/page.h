#pragma once

#include <stdint.h>
#include <stddef.h>

void page_init(void);
int page_getnfree(void);
void *page_alloc(void);
void page_free(uint32_t addr);
void bzero(void *s, size_t n);
void *get_zeropage();
