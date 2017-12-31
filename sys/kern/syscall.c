#include <kern/syscall.h>

void syscall_exit(u32, u32, u32, u32, u32);
void syscall_helloworld(u32, u32, u32, u32, u32);
 
void (*syscall_table[NSYSCALLS])(u32, u32, u32, u32, u32) = {
  syscall_exit,
  syscall_helloworld,
};


void syscall_exit(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  thread_exit();
}

void syscall_helloworld(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  puts("hello, world!");
}
