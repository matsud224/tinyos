#pragma once

#include <stddef.h>
#include <stdint.h>

int rtl8139_probe(void);
void rtl8139_isr(void);
void rtl8139_inthandler(void);
