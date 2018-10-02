#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  struct stat stbuf;
  if(stat(argv[1], &stbuf) < 0) {
    puts("failed");
    return -1;
  }
  printf("ino = %d\n", stbuf.st_ino);
  printf("mode = %x\n", stbuf.st_mode);
  printf("nlink = %d\n", stbuf.st_nlink);
  printf("size = %d\n", (int)stbuf.st_size);
  return 0;
}
