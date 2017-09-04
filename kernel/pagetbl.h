#pragma once

#include "pagetbl.h"
#include <stdint.h>
#include <stddef.h>

extern uint32_t *current_pdt;

void pagetbl_init(void);
