#include "serial.h"
#include "pic.h"
#include "chardev.h"
#include "kernasm.h"
#include "idt.h"
#include "kernlib.h"

#define COMPORT_NUM 2

#define SERIAL_BUFSIZE 64

enum {
  COM_DATA				= 0,
  COM_INTENABLE		= 1,
  COM_DIVISOR_LO	= 0,
  COM_DIVISOR_HI	= 1,
  COM_INT_FIFO_CTRL,
  COM_LINECTRL,
  COM_MODEMCTRL,
  COM_LINESTAT,
  COM_MODEMSTAT,
  COM_SCRATCH
};

#define COM_INTENABLE_DATAAVAIL	0x1
#define COM_INTENABLE_TXEMPTY		0x2
#define COM_INTENABLE_BRKERR		0x4
#define COM_INTENABLE_STCHANGE	0x8

static void serial_open(struct chardev *dev);
static void serial_close(struct chardev *dev);
static uint32_t serial_read(struct chardev *dev, uint8_t *dest, uint32_t count);
static uint32_t serial_write(struct chardev *dev, uint8_t *src, uint32_t count);
 
static struct chardev_ops serial_chardev_ops = {
  .open = serial_open,
  .close = serial_close,
  .read = serial_read,
  .write = serial_write
};

static struct comport{
  uint8_t port;
  uint16_t base;
  uint8_t irq;
  uint8_t intvec;
  void (*inthandler)(void);
  struct chardev_buf *rxbuf;
  struct chardev_buf *txbuf;
  struct chardev chardev_info;
} comport[COMPORT_NUM] = {
  {.port = 0, .base = 0x3f8, .irq = 4, .intvec = 0x24, .inthandler = com1_inthandler, .chardev_info.ops = &serial_chardev_ops},
  {.port = 1, .base = 0x2f8, .irq = 3, .intvec = 0x23, .inthandler = com2_inthandler, .chardev_info.ops = &serial_chardev_ops}
};


void serial_init() {
  for(int i=0; i<COMPORT_NUM; i++) {
    uint16_t base = comport[i].base;

    idt_register(comport[i].intvec, IDT_INTGATE, comport[i].inthandler);

    out8(base + COM_INTENABLE,			0x03);
    out8(base + COM_LINECTRL,				0x80);
    out8(base + COM_DIVISOR_LO,			0x03);
    out8(base + COM_DIVISOR_HI, 		0x00);
    out8(base + COM_LINECTRL,				0x03);
    out8(base + COM_INT_FIFO_CTRL,	0xc7);
    out8(base + COM_MODEMCTRL,			0x0b);

    comport[i].rxbuf = chardevbuf_create(malloc(SERIAL_BUFSIZE), SERIAL_BUFSIZE);
    comport[i].txbuf = chardevbuf_create(malloc(SERIAL_BUFSIZE), SERIAL_BUFSIZE);

    pic_clearmask(comport[i].irq);
    chardev_add(&comport[i].chardev_info);
  }
}

static void serial_send(int port) {
  uint16_t base = comport[port].base;
  uint8_t data;
  if(CHARDEVBUF_IS_FULL(comport[port].txbuf))
    task_wakeup(&comport[port].chardev_info);
  if(chardevbuf_read(comport[port].txbuf, &data, 1) == 1)
    out8(base+COM_DATA, data);
}

void serial_isr_common(int port) {
  uint16_t base = comport[port].base;
  uint8_t reason = in8(base+COM_INT_FIFO_CTRL);
  uint8_t data;
  if((reason & 0x1) == 0) {
    switch((reason & 0x6)>>1) {
    case 1:
      //送信準備完了
      if(CHARDEVBUF_IS_EMPTY(comport[port].txbuf))
        in8(base+COM_INT_FIFO_CTRL);
      else
        serial_send(port);
      break;
    case 2:
    case 6:
      //受信
      data = in8(base+COM_DATA);
      if(CHARDEVBUF_IS_EMPTY(comport[port].rxbuf))
        task_wakeup(&comport[port].chardev_info);
      if(!CHARDEVBUF_IS_FULL(comport[port].rxbuf))
        chardevbuf_write(comport[port].rxbuf, &data, 1);
      break;
    case 3:
      in8(base+COM_LINESTAT);
      break;
    }
  }
  
  pic_sendeoi();
  task_yield();
}

void com1_isr() {
  serial_isr_common(0);
}

void com2_isr() {
  serial_isr_common(1);
}


static void serial_open(struct chardev *dev) {
  return;
}

static void serial_close(struct chardev *dev) {
  return;
}

static uint32_t serial_read(struct chardev *dev, uint8_t *dest, uint32_t count) {
  struct comport *com = container_of(dev, struct comport, chardev_info);
  uint32_t n = chardevbuf_read(com->rxbuf, dest, count);
  return n;
}

static uint32_t serial_write(struct chardev *dev, uint8_t *src, uint32_t count) {
  struct comport *com = container_of(dev, struct comport, chardev_info);
  uint32_t n = chardevbuf_write(com->txbuf, src, count);
  serial_send(com->port);
  return n;
}

