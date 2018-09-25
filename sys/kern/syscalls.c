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
u32 syscall_dup(u32, u32, u32, u32, u32);
u32 syscall_chdir(u32, u32, u32, u32, u32);

u32 (*syscall_table[NSYSCALLS])(u32, u32, u32, u32, u32) = {
  syscall_exit,     //0
  syscall_write,    //1
  syscall_read,     //2
  syscall_getpid,   //3
  syscall_isatty,   //4
  syscall_close,    //5
  syscall_execve,   //6
  syscall_fork,     //7
  syscall_stat,     //8
  syscall_fstat,    //9
  syscall_kill,     //10
  syscall_link,     //11
  syscall_lseek,    //12
  syscall_open,     //13
  syscall_sbrk,     //14
  syscall_times,    //15
  syscall_unlink,   //16
  syscall_wait,     //17
  syscall_socket,   //18
  syscall_bind,     //19
  syscall_connect,  //20
  syscall_listen,   //21
  syscall_accept,   //22
  syscall_sendto,   //23
  syscall_recvfrom, //24
  syscall_send,     //25
  syscall_recv,     //26
  syscall_dup,      //27
  syscall_getdents, //28
  syscall_chdir,    //29
};


int address_check(const void *addr UNUSED) {
  return 0; //TODO
}

int buffer_check(const void *buf UNUSED, size_t count UNUSED) {
  return 0; //TODO
}

u32 syscall_exit(u32 a0, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  thread_exit(a0);
  return 0;
}

u32 syscall_write(u32 a0, u32 a1, u32 a2, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_write(a0, (void *)a1, a2);
}

u32 syscall_read(u32 a0, u32 a1, u32 a2, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_read(a0, (void *)a1, a2);
}

u32 syscall_getpid(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return current->pid;
}

u32 syscall_isatty(u32 a0, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_isatty(a0);
}

u32 syscall_close(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_close(a0);
}

u32 syscall_execve(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_execve(a0, a1, a2);
}

u32 syscall_fork(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_fork();
}

u32 syscall_stat(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_stat((void *)a0, (void *)a1);
}

u32 syscall_fstat(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_fstat(a0, (void *)a1);
}

u32 syscall_kill(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return -1;
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

u32 syscall_sbrk(u32 a0, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_sbrk(a0);
}

u32 syscall_times(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return -1;
}

u32 syscall_unlink(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_unlink((void *)a0);
}

u32 syscall_wait(u32 a0 UNUSED, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_wait(a0);
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

u32 syscall_dup(u32 a0, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_dup(a0);
}

u32 syscall_chdir(u32 a0, u32 a1 UNUSED, u32 a2 UNUSED, u32 a3 UNUSED, u32 a4 UNUSED) {
  return sys_chdir(a0);
}
