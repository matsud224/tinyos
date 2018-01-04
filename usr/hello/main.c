#include "syscall.h"
#include <unistd.h>
#include <stdio.h>

int a = 2;

void func() {
  if(a)
    syscall1();
}

int main() {
  //printf("%d\n", 90);
  syscall1();
  //func();
  return 123;
}
