#include "kernasm.h"
#include "pic.h"

enum regs {
  MASTER_CMD		= 0x20,
  MASTER_STAT		= 0x20,
  MASTER_IMR		= 0x21,
  MASTER_DATA		= 0x21,
  SLAVE_CMD			= 0xa0,
  SLAVE_STAT		= 0xa0,
  SLAVE_IMR			= 0xa1,
  SLAVE_DATA		= 0xa1,
};

enum mask {
  MASK_IRQ0		= 0x01,
  MASK_IRQ1		= 0x02,
  MASK_IRQ2		= 0x04,
  MASK_IRQ3		= 0x08,
  MASK_IRQ4		= 0x10,
  MASK_IRQ5		= 0x20,
  MASK_IRQ6		= 0x40,
  MASK_IRQ7		= 0x80,
  MASK_ALL		= 0xff,
};

static u8 master_imr;
static u8 slave_imr;

void pic_init() {
  // send ICW1
  out8(MASTER_CMD, 0x11);
  out8(SLAVE_CMD, 0x11);
  // send ICW2
  out8(MASTER_DATA, 0x20);
  out8(SLAVE_DATA, 0x28);
  // send ICW3
  out8(MASTER_DATA, 0x04);
  out8(SLAVE_DATA, 0x02);
  // send ICW4
  out8(MASTER_DATA, 0x01);
  out8(SLAVE_DATA, 0x01);  
  // set IMR
  master_imr = ~MASK_IRQ2;
  slave_imr = MASK_ALL;
  out8(MASTER_IMR, master_imr);
  out8(SLAVE_IMR, slave_imr);
}

void pic_setmask(int irq) {
  if(irq < 8) {
    master_imr |= 1<<irq;
    out8(MASTER_IMR, master_imr);
  } else {
    slave_imr |= 1<<(irq-8);
    out8(SLAVE_IMR, slave_imr);
  }
}

void pic_clearmask(int irq) {
  if(irq < 8) {
    master_imr &= ~(1<<irq);
    out8(MASTER_IMR, master_imr);
  } else {
    slave_imr &= ~(1<<(irq-8));
    out8(SLAVE_IMR, slave_imr);
  }
}

void pic_sendeoi() {
  out8(MASTER_CMD, 0x20);
  out8(SLAVE_CMD, 0x20);
}
