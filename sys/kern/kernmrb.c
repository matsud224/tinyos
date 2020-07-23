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

static void dump_error(mrb_state *mrb) {
  mrb_print_error(mrb);
  mrb->exc = 0;
}

int kernelmrb_load_string(const char *code) {
  if (!global_state)
    return -1;

  int arena = mrb_gc_arena_save(global_state);
  mrbc_context *ctx = mrbc_context_new(global_state);
  mrbc_filename(global_state, ctx, "*kernel-interactive*");
  mrb_load_string_cxt(global_state, code, ctx);
  mrbc_context_free(global_state, ctx);
  if (global_state->exc)
    dump_error(global_state);
  mrb_gc_arena_restore(global_state, arena);
  return 0;
}

int kernelmrb_load_irep(u8 *bin, const char *filename) {
  if (!global_state)
    return -1;

  int arena = mrb_gc_arena_save(global_state);
  mrbc_context *ctx = mrbc_context_new(global_state);
  printf("mruby: loading %s...\n", filename);
  mrbc_filename(global_state, ctx, filename);
  mrb_load_irep_cxt(global_state, bin, ctx);
  mrbc_context_free(global_state, ctx);
  if (global_state->exc)
    dump_error(global_state);
  mrb_gc_arena_restore(global_state, arena);
  return 0;
}

void kernelmrb_load_all_builtin_scripts() {
  int count = sizeof(mrbscript_irep_list) / sizeof(char *);

  for (int i = 0; i < count; i++) {
    kernelmrb_load_irep(mrbscript_irep_list[i], mrbscript_filename_list[i]);
  }
}
