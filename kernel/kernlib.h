#pragma once

#include <stdint.h>
#include <stddef.h>
#include "params.h"
#include "types.h"
#include "list.h"
#include "kernasm.h"

#define INLINE __inline__
#define KERNENTRY __attribute__ ((section (".entry")))
#define PACKED __attribute__ ((packed))
#define ASM __asm__ __volatile__
#define container_of(ptr, type, member) ({ \
          const typeof(((type *)0)->member) *__mptr=(ptr); \
          (type *)((char*)__mptr-offsetof(type, member));})

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

int abs(int n);
char *strncpy(char *dest, const char *src, size_t n);
size_t strnlen(const char *s, size_t n);
int strncmp(const char *s1, const char *s2, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void bzero(void *s, size_t n);
void *memset(void *s, int c, size_t n);
void *malloc(size_t request);
void free(void *addr);
int putchar(int c);
int puts(const char *str);
void printf(const char *fmt, ...);
