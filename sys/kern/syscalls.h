#include <kern/kernlib.h>

#define NSYSCALLS 30

extern u32 (*syscall_table[NSYSCALLS])(u32, u32, u32, u32, u32);

int address_check(const void *addr);
int buffer_check(const void *buf, size_t count);
