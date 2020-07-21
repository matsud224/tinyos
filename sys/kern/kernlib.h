#pragma once
#include <stdint.h>
#include <stddef.h>
#include <kern/params.h>
#include <kern/types.h>
#include <kern/list.h>
#include <kern/queue.h>
#include <kern/kernasm.h>

#define INLINE __inline__
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

typedef int jmp_buf[6];

int abs(int n);
char *strncpy(char *dest, const char *src, size_t n);
size_t strnlen(const char *s, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strdup(const char *str);
void *memcpy(void *dest, const void *src, size_t n);
void bzero(void *s, size_t n);
void *memset(void *s, int c, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);
void *memmove(void *dest, const void *src, size_t n);
void *memchr(const void *s, int c, size_t n);
char *strchr(const char *s, int c);
char *strcpy(char *dest, const char *src);
char *strncat(char *dest, const char *src, size_t n);
char *strcat(char *dest, const char *src);
int isspace(int c);
unsigned long int strtoul(const char *nptr, char **endptr, int base);
int toupper(int c);
int tolower(int c);
int setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);
void *malloc(size_t request);
void *realloc(void *ptr, size_t size);
void free(void *addr);
int putchar(int c);
int puts(const char *str);
void printf(const char *fmt, ...);
void show_line(void);
void show_number(u32);
void abort(void);
void exit(int status);
void panic(const char *msg);
