#include "kernasm.h"
#include "pic.h"
#include <stdint.h>

#define PIC_MASTER_CMD 0x20
#define PIC_MASTER_STAT 0x20
#define PIC_MASTER_IMR 0x21
#define PIC_MASTER_DATA 0x21
#define PIC_SLAVE_CMD 0xa0
#define PIC_SLAVE_STAT 0xa0
#define PIC_SLAVE_IMR 0xa1
#define PIC_SLAVE_DATA 0xa1

#define PIC_MASK_IRQ0 0x01
#define PIC_MASK_IRQ1 0x02
#define PIC_MASK_IRQ2 0x04
#define PIC_MASK_IRQ3 0x08
#define PIC_MASK_IRQ4 0x10
#define PIC_MASK_IRQ5 0x20
#define PIC_MASK_IRQ6 0x40
#define PIC_MASK_IRQ7 0x80
#define PIC_MASK_ALL 0xff

static uint8_t master_imr;
static uint8_t slave_imr;

void pic_init() {
  // send ICW1
  out8(PIC_MASTER_CMD, 0x11);
  out8(PIC_SLAVE_CMD, 0x11);
  // send ICW2
  out8(PIC_MASTER_DATA, 0x20);
  out8(PIC_SLAVE_DATA, 0x28);
  // send ICW3
  out8(PIC_MASTER_DATA, 0x04);
  out8(PIC_SLAVE_DATA, 0x02);
  // send ICW4
  out8(PIC_MASTER_DATA, 0x01);
  out8(PIC_SLAVE_DATA, 0x01);  
  // set IMR
  master_imr = ~PIC_MASK_IRQ2;
  slave_imr = PIC_MASK_ALL;
  out8(PIC_MASTER_IMR, master_imr);
  out8(PIC_SLAVE_IMR, slave_imr);
}

void pic_setmask_master(int irq) {
  master_imr |= 1<<irq;
  out8(PIC_MASTER_IMR, master_imr);
}

void pic_setmask_slave(int irq) {
  slave_imr |= 1<<irq;
  out8(PIC_SLAVE_IMR, slave_imr);
}

void pic_clearmask_master(int irq) {
  master_imr &= ~(1<<irq);
  out8(PIC_MASTER_IMR, master_imr);
}

void pic_clearmask_slave(int irq) {
  slave_imr &= ~(1<<irq);
  out8(PIC_SLAVE_IMR, slave_imr);
}

void pic_sendeoi() {
  out8(PIC_MASTER_CMD, 0x20);
  out8(PIC_SLAVE_CMD, 0x20);
}
