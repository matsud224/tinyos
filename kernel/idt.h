#pragma once

#include "common.h"
#include <stdint.h>

#define IDT_INTGATE 0x6
#define IDT_TRAPGATE 0x7

void idt_register(uint8_t vecnum, uint8_t gatetype, void (*base)(void));
void idt_unregister(uint8_t vecnum);
void idt_init(void);
