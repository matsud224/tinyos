#include <kern/kernmrb.h>
#include <kern/kernlib.h>
#include <mruby.h>
#include <mruby/compile.h>

static mrb_state *global_state = NULL;

void kernelmrb_init() {
  global_state = mrb_open();
  if (!global_state)
    panic("failed to initialize mruby");
}

int kernelmrb_load_string(const char *code) {
  if (!global_state)
    return -1;

  mrb_load_string(global_state, code);
  return 0;
}

