#include <kern/kernlib.h>

int __errno = 0;

void abort_for_mrb(void) {
  puts("mruby aborted.");
  while(1);
}

void exit_for_mrb(int status) {
  printf("mruby exited (%d).\n", status);
  while(1);
}
