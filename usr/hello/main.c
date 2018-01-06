#include "syscall.h"
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <malloc.h>

int a = 2;

void func() {
  if(a)
    syscall1();
}

int main() {
  char c[10] = "111111";
  puts("hello, world!!");
  printf("%s %d\n", c, strlen(c));
  printf("a is %d\n", a);
  printf("pow: %d\n", (int)pow(a, 5));
  a = 100;
  printf("a is %10x\n", a);
  return 123;
}
