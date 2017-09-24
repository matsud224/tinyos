#pragma once

#include <stdint.h>
#include <stddef.h>
#include "task.h"

void gdt_init(void);
void gdt_settssbase(void *base);
