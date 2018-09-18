#include <kern/syscalls.h>
#include <kern/kernlib.h>
#include <kern/thread.h>
#include <kern/fs.h>
#include <kern/file.h>
#include <net/socket/socket.h>

u32 syscall_exit(u32, u32, u32, u32, u32);
u32 syscall_write(u32, u32, u32, u32, u32);
u32 syscall_read(u32, u32, u32, u32, u32);
u32 syscall_getpid(u32, u32, u32, u32, u32);
u32 syscall_isatty(u32, u32, u32, u32, u32);
u32 syscall_close(u32, u32, u32, u32, u32);
u32 syscall_execve(u32, u32, u32, u32, u32);
u32 syscall_fork(u32, u32, u32, u32, u32);
u32 syscall_stat(u32, u32, u32, u32, u32);
u32 syscall_fstat(u32, u32, u32, u32, u32);
u32 syscall_kill(u32, u32, u32, u32, u32);
u32 syscall_link(u32, u32, u32, u32, u32);
u32 syscall_lseek(u32, u32, u32, u32, u32);
u32 syscall_open(u32, u32, u32, u32, u32);
u32 syscall_sbrk(u32, u32, u32, u32, u32);
u32 syscall_times(u32, u32, u32, u32, u32);
u32 syscall_unlink(u32, u32, u32, u32, u32);
u32 syscall_wait(u32, u32, u32, u32, u32);
u32 syscall_socket(u32, u32, u32, u32, u32);
u32 syscall_bind(u32, u32, u32, u32, u32);
u32 syscall_connect(u32, u32, u32, u32, u32);
u32 syscall_listen(u32, u32, u32, u32, u32);
u32 syscall_accept(u32, u32, u32, u32, u32);
u32 syscall_sendto(u32, u32, u32, u32, u32);
u32 syscall_recvfrom(u32, u32, u32, u32, u32);
u32 syscall_send(u32, u32, u32, u32, u32);
u32 syscall_recv(u32, u32, u32, u32, u32);
u32 syscall_sync(u32, u32, u32, u32, u32);
u32 syscall_truncate(u32, u32, u32, u32, u32);
u32 syscall_getdents(u32, u32, u32, u32, u32);

u32 (*syscall_table[NSYSCALLS])(u32, u32, u32, u32, u32) = {
  syscall_exit,
  syscall_write,
  syscall_read,
  syscall_getpid,
  syscall_isatty,
  syscall_close,
  syscall_execve,
  syscall_fork,
  syscall_stat,
  syscall_fstat,
  syscall_kill,
  syscall_link,
  syscall_lseek,
  syscall_open,
  syscall_sbrk,
  syscall_times,
  syscall_unlink,
  syscall_wait,
  syscall_socket,
  syscall_bind,
  syscall_connect,
  syscall_listen,
  syscall_accept,
  syscall_sendto,
  syscall_recvfrom,
  syscall_send,
  syscall_recv,
};


int address_check(const void *addr UNUSED) {
  return 0; //TODO
}

int buffer_check(const void *buf UNUSED, size_t count UNUSED) {
  return 0; //TODO
}

u32 syscall_exit(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  thread_exit();
  return 0;
}

u32 syscall_write(u32 a0, u32 a1, u32 a2, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_write(a0, (void *)a1, a2);
}

u32 syscall_read(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_read(a0, (void *)a1, a2);
}

u32 syscall_getpid(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return current->pid;
}

u32 syscall_isatty(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
}

u32 syscall_close(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_close(a0);
}

u32 syscall_execve(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
}

u32 syscall_fork(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

u32 syscall_stat(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_stat((void *)a0, (void *)a1);
}

u32 syscall_fstat(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_fstat(a0, (void *)a1);
}

u32 syscall_kill(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
}

u32 syscall_link(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_link((void *)a0, (void *)a1);
}

u32 syscall_lseek(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

  return sys_lseek(a0, a1, a2);
}

u32 syscall_open(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_open((void *)a0, a1);
}

u32 syscall_sbrk(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

u32 syscall_times(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

u32 syscall_unlink(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_unlink((void *)a0);
}

u32 syscall_wait(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {

}

u32 syscall_socket(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_socket(a0, a1);
}

u32 syscall_bind(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_bind(a0, (void *)a1);
}

u32 syscall_connect(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_connect(a0, (void *)a1);
}

u32 syscall_listen(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_listen(a0, a1);
}

u32 syscall_accept(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_accept(a0, (void *)a1);
}

u32 syscall_sendto(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_sendto(a0, (void *)a1, a2, a3, (void *)a4);
}

u32 syscall_recvfrom(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_recvfrom(a0, (void *)a1, a2, a3, (void *)a4);
}

u32 syscall_send(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_send(a0, (void *)a1, a2, a3);
}

u32 syscall_recv(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_recv(a0, (void *)a1, a2, a3);
}

u32 syscall_fsync(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_fsync(a0);
}

u32 syscall_truncate(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_truncate(a0, a1);
}

u32 syscall_getdents(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_getdents(a0, (void *)a1, a2);
}




