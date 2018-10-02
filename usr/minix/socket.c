#include "syscall.h"
#include "socket.h"
#include <sys/types.h>

int socket(int domain, int type) {
  return syscall_2(18, domain, type);
}

int bind(int fd, const struct sockaddr *addr) {
  return syscall_2(19, fd, addr);
}

int sendto(int fd, const char *msg, size_t len, int flags, const struct sockaddr *to_addr) {
  return syscall_5(23, fd, msg, len, flags, to_addr);
}

int recvfrom(int fd, char *buf, size_t len, int flags, struct sockaddr *from_addr) {
  return syscall_5(24, fd, buf, len, flags, from_addr);
}

int connect(int fd, const struct sockaddr *to_addr) {
  return syscall_2(20, fd, to_addr);
}

int listen(int fd, int backlog) {
  return syscall_2(21, fd, backlog);
}

int accept(int fd, struct sockaddr *client_addr) {
  return syscall_2(22, fd, client_addr);
}

int send(int fd, const char *msg, size_t len, int flags) {
  return syscall_4(25, fd, msg, len, flags);
}

int recv(int fd, char *buf, size_t len, int flags) {
  return syscall_4(26, fd, buf, len, flags);
}

