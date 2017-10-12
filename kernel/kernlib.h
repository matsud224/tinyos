#pragma once

#include <stdint.h>
#include <stddef.h>
#include "params.h"
#include "common.h"
#include "list.h"

#define MAX(a,b) ((a)>(b)?(a):(b))
#define MIN(a,b) ((a)<(b)?(a):(b))

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
