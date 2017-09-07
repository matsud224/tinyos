#include "pit.h"
#include "pic.h"
#include "idt.h"
#include "vga.h"
#include "kernasm.h"
#include <stdint.h>

#define PIT_CH0_DATA 0x40
#define PIT_CH1_DATA 0x41
#define PIT_CH2_DATA 0x42
#define PIT_MODE_CMD_REG 0x43

#define PIT_CNTMODE_BIN 0x0
#define PIT_OPMODE_RATE 0x4
#define PIT_OPMODE_SQUARE 0x6
#define PIT_LOAD16 0x30
#define PIT_CNT0 0x0

#define PIT_IRQ 0 
#define PIT_INT_VEC 0x20

#define CNT_100HZ 0x2e9c


void pit_inthandler() {
  puts("IRQ0");
  pic_sendeoi();
}

void pit_init() {
  out8(PIT_MODE_CMD_REG,
        PIT_CNTMODE_BIN | PIT_OPMODE_RATE | PIT_LOAD16 | PIT_CNT0);
  out8(PIT_CH0_DATA, CNT_100HZ >> 8);
  out8(PIT_CH0_DATA, CNT_100HZ & 0xff);
  idt_register(PIT_INT_VEC, IDT_INTGATE, pit_isr);
  pic_clearmask(PIT_IRQ);
}
