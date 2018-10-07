#include "tinyos.h"
#include "syscall.h"
#include <sys/types.h>

int getdents(int fd, struct dirent *dirp, size_t count) {
  return syscall_3(28, fd, dirp, count);
}

int gettents(struct threadent *thp, size_t count) {
  return syscall_2(32, thp, count);
}

int getsents(struct sockent *sockp, size_t count) {
  return syscall_2(33, sockp, count);
}
