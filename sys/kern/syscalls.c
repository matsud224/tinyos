#include <kern/syscalls.h>

void syscall_exit(u32, u32, u32, u32, u32);
void syscall_write(u32, u32, u32, u32, u32);
void syscall_read(u32, u32, u32, u32, u32);
void syscall_getpid(u32, u32, u32, u32, u32);
void syscall_isatty(u32, u32, u32, u32, u32);
void syscall_close(u32, u32, u32, u32, u32);
void syscall_execve(u32, u32, u32, u32, u32);
void syscall_fork(u32, u32, u32, u32, u32);
void syscall_fstat(u32, u32, u32, u32, u32);
void syscall_kill(u32, u32, u32, u32, u32);
void syscall_link(u32, u32, u32, u32, u32);
void syscall_lseek(u32, u32, u32, u32, u32);
void syscall_open(u32, u32, u32, u32, u32);
void syscall_sbrk(u32, u32, u32, u32, u32);
void syscall_times(u32, u32, u32, u32, u32);
void syscall_unlink(u32, u32, u32, u32, u32);
void syscall_wait(u32, u32, u32, u32, u32);
void syscall_bind(u32, u32, u32, u32, u32);
void syscall_connect(u32, u32, u32, u32, u32);
void syscall_listen(u32, u32, u32, u32, u32);
void syscall_accept(u32, u32, u32, u32, u32);
void syscall_sendto(u32, u32, u32, u32, u32);
void syscall_recvfrom(u32, u32, u32, u32, u32);
 
void (*syscall_table[NSYSCALLS])(u32, u32, u32, u32, u32) = {
  syscall_exit,
  syscall_write,
  syscall_read,
  syscall_getpid,
  syscall_isatty,
  syscall_close,
  syscall_execve,
  syscall_fork,
  syscall_fstat,
  syscall_kill,
  syscall_link,
  syscall_lseek,
  syscall_open,
  syscall_sbrk,
  syscall_times,
  syscall_unlink,
  syscall_wait,
  syscall_bind,
  syscall_connect,
  syscall_listen,
  syscall_accept,
  syscall_sendto,
  syscall_recvfrom,
};


void syscall_exit(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  thread_exit();
}

void syscall_write(u32 a0, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  printf("%c", a0);
}

void syscall_read(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_getpid(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_isatty(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_close(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_execve(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_fork(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_fstat(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_kill(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_link(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_lseek(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_open(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_sbrk(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_times(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_unlink(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_wait(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_bind(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_connect(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_listen(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_accept(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_sendto(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

void syscall_recvfrom(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

 
