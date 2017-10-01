#pragma once
#include <stdint.h>
#include <stddef.h>

void serial_init(void);

void com1_inthandler(void);
void com2_inthandler(void);
void com1_isr(void);
void com2_isr(void);
