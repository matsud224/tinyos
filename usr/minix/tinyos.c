#include "tinyos.h"
#include "syscall.h"
#include <sys/types.h>

int getdents(int fd, struct dirent *dirp, size_t count) {
  return syscall_3(28, fd, dirp, count);
}


