#include "syscall.h"
#include <stdio.h>

int main() {
  int n;
  printf("input number: ");
  fflush(stdout);
  scanf("%d", &n);
  switch(n) {
    case 0:
      do_cli();
      break;
    case 1:
      *(int *)(0xc0000000) = 0x12345678;
      break;
    case 2:
      printf("%x\n", *(int *)(0xc0000000));
      break;
  }

  return 0;
}
