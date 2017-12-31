#include "syscall.h"

int main() {
  syscall1();
  do_cli();
  exit();
  return 123;
}
