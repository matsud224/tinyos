#include "syscall.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>
#include <string.h>

int main() {
  puts("fork test");
  printf("--- --- --- --- ---\n");
  int ret = fork();
  printf("fork() = %d\n", ret);

  if(ret == 0) {
    int x;
    puts("child process");
    scanf("%d", &x);
    return x;
  } else {
    int status;
    puts("parent process");
    int pid = wait(&status);
    printf("parent: child pid = %d return with %d\n", pid, status);
  }
  return 0;
}
