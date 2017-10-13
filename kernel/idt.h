#pragma once
#include "kernlib.h"

#define IDT_INTGATE 0x6
#define IDT_TRAPGATE 0x7

void idt_register(u8 vecnum, u8 gatetype, void (*base)(void));
void idt_unregister(u8 vecnum);
void idt_init(void);
