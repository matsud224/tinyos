#include <kern/kernlib.h>

int __errno = 0;

void abort_for_mrb(void) {
  puts("mruby aborted.");
}

void exit_for_mrb(int status) {
  printf("mruby exited (%d).\n", status);
}
