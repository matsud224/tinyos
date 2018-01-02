#include "syscall.h"

int a = 2;

int func() {
  if(a)
    syscall1();
}

int main() {
  func();
  exit();
  return 123;
}
