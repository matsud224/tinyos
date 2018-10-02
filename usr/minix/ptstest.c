#include "syscall.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
  int ret = fork();
  if(ret == 0) {
    int b;
    for(int a=0; a<0x0; a++)
      b = toupper(a);
    char *ptsname = argv[1];
    int fd = open(ptsname, O_RDWR);
    if(fd < 0) {
      return -1;
    }

    int c;
    while(read(fd, &c, 1) >= 0) {
      c = toupper(c);
      write(fd, &c, 1);
    }
  }
  return 0;
}
