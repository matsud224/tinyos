#pragma once
#include <kern/types.h>

void kernelmrb_init(void);
int kernelmrb_load_string(const char *);
int kernelmrb_load_irep(u8 *bin, const char *);
void kernelmrb_load_all_builtin_scripts(void);
