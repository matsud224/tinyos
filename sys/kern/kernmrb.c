#include <kern/kernmrb.h>
#include <kern/kernlib.h>
#include <mruby.h>
#include <mruby/irep.h>
#include <mruby/compile.h>

#include "mrbscript_list.h"

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

int kernelmrb_load_irep(u8 *bin) {
  if (!global_state)
    return -1;

  mrb_load_irep(global_state, bin);
  return 0;
}

void kernelmrb_load_all_builtin_scripts() {
  int count = sizeof(mrbscript_irep_list) / sizeof(char *);

  for (int i = 0; i < count; i++) {
    kernelmrb_load_irep(mrbscript_irep_list[i]);
  }
}
