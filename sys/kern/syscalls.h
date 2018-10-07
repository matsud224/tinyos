#include <kern/kernlib.h>

#define NSYSCALLS 34

extern u32 (*syscall_table[NSYSCALLS])(u32, u32, u32, u32, u32);

int string_check(const char *addr);
int buffer_check(const void *buf, size_t count);
