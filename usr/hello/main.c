#include "syscall.h"
#include <unistd.h>
#include <stdio.h>

int a = 2;

void func() {
  if(a)
    syscall1();
}

int main() {
  if(isatty(0) == 45)
    syscall1();
  return 123;
}
