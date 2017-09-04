#pragma once

#include <stdint.h>

void divzero(void);
void gengpe(void);
void out8(uint16_t port, uint8_t val);
void out16(uint16_t port, uint16_t val);
void out32(uint16_t port, uint32_t val);
uint8_t in8(uint16_t port);
uint16_t in16(uint16_t port);
uint32_t in32(uint16_t port);
void lidt(void *p);
void sti(void);
void cli(void);
uint32_t getcr2(void);
void flushtlb(void);
