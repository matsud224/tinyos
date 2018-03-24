#pragma once
#include <stdint.h>
#include <stddef.h>
#include <kern/params.h>
#include <kern/types.h>
#include <kern/list.h>
#include <kern/queue.h>
#include <kern/kernasm.h>

#define INLINE __inline__
#define KERNENTRY __attribute__ ((section (".entry")))
#define PACKED __attribute__ ((packed))
#define UNUSED __attribute__ ((unused))
#define ASM __asm__ __volatile__
#define container_of(ptr, type, member) ({ \
          const typeof(((type *)0)->member) *__mptr=(ptr); \
          (type *)((char*)__mptr-offsetof(type, member));})

#define DRIVER_INIT __attribute__((constructor))
#define FS_INIT __attribute__((constructor))
#define NET_INIT __attribute__((constructor))

#define MAX(a, b) ((a)>(b)?(a):(b))
#define MIN(a, b) ((a)<(b)?(a):(b))

#define IRQ_DISABLE do{ int __ie = geteflags()&0x200; if(__ie) cli();
#define IRQ_RESTORE if(__ie) sti(); }while(0);

#define pagealign(a) ((a)&~(PAGESIZE-1))
#define align(a, b) ((a)&~(b-1))

#define DEV_MINOR(n) ((n) & 0xff)
#define DEV_MAJOR(n) ((n) >> 8)
#define DEVNO(ma, mi) (((ma)<<8) | (mi))
#define BAD_MAJOR 0

int abs(int n);
char *strncpy(char *dest, const char *src, size_t n);
size_t strnlen(const char *s, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strdup(const char *str);
void *memcpy(void *dest, const void *src, size_t n);
void bzero(void *s, size_t n);
void *memset(void *s, int c, size_t n);
void *malloc(size_t request);
void free(void *addr);
int putchar(int c);
int puts(const char *str);
void printf(const char *fmt, ...);
