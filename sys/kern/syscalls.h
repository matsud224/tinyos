#include <kern/kernlib.h>

#define NSYSCALLS 23

extern void (*syscall_table[NSYSCALLS])(u32, u32, u32, u32, u32);
