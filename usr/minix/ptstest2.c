#include "syscall.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
  char *ptsname = argv[1];
  int fd = open(ptsname, O_RDWR);
  if(fd < 0) {
    return -1;
  }

  int c;
  while((c = fgetc(stdin)) >= 0) {
    if(write(fd, &c, 1) < 0)
      puts("write failed");
    if(read(fd, &c, 1) < 0)
      puts("read failed");
    putchar(c);
  }
  return 0;
}
