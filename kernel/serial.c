#include "serial.h"
#include "pic.h"
#include "chardev.h"
#include "kernasm.h"
#include "idt.h"
#include "kernlib.h"
#include "task.h"

#define COMPORT_NUM 2

#define SERIAL_BUFSIZE 64

enum {
  DATA				= 0,
  INTENABLE		= 1,
  DIVISOR_LO	= 0,
  DIVISOR_HI	= 1,
  INT_FIFO_CTRL,
  LINECTRL,
  MODEMCTRL,
  LINESTAT,
  MODEMSTAT,
  SCRATCH
};

enum ie {
  IE_DATAAVAIL	= 0x1,
  IE_TXEMPTY		= 0x2,
  IE_BRKERR			= 0x4,
  IE_STCHANGE		= 0x8,
};

void com1_inthandler(void);
void com2_inthandler(void);
void com1_isr(void);
void com2_isr(void);

static void serial_open(struct chardev *dev);
static void serial_close(struct chardev *dev);
static u32 serial_read(struct chardev *dev, u8 *dest, u32 count);
static u32 serial_write(struct chardev *dev, u8 *src, u32 count);
 
static const struct chardev_ops serial_chardev_ops = {
  .open = serial_open,
  .close = serial_close,
  .read = serial_read,
  .write = serial_write
};

static struct comport{
  u8 port;
  u16 base;
  u8 irq;
  u8 intvec;
  void (*inthandler)(void);
  struct chardev_buf *rxbuf;
  struct chardev_buf *txbuf;
  struct chardev chardev_info;
} comport[COMPORT_NUM] = {
  {.port = 0, .base = 0x3f8, .irq = 4, .intvec = IRQ_TO_INTVEC(4), .inthandler = com1_inthandler, .chardev_info.ops = &serial_chardev_ops},
  {.port = 1, .base = 0x2f8, .irq = 3, .intvec = IRQ_TO_INTVEC(3), .inthandler = com2_inthandler, .chardev_info.ops = &serial_chardev_ops}
};


void serial_init() {
  for(int i=0; i<COMPORT_NUM; i++) {
    u16 base = comport[i].base;

    idt_register(comport[i].intvec, IDT_INTGATE, comport[i].inthandler);

    out8(base + INTENABLE,			0x03);
    out8(base + LINECTRL,				0x80);
    out8(base + DIVISOR_LO,			0x03);
    out8(base + DIVISOR_HI, 		0x00);
    out8(base + LINECTRL,				0x03);
    out8(base + INT_FIFO_CTRL,	0xc7);
    out8(base + MODEMCTRL,			0x0b);

    comport[i].rxbuf = cdbuf_create(malloc(SERIAL_BUFSIZE), SERIAL_BUFSIZE);
    comport[i].txbuf = cdbuf_create(malloc(SERIAL_BUFSIZE), SERIAL_BUFSIZE);

    pic_clearmask(comport[i].irq);
    chardev_add(&comport[i].chardev_info);
  }
}

static void serial_send(int port) {
  u16 base = comport[port].base;
  u8 data;
  if(CDBUF_IS_FULL(comport[port].txbuf))
    task_wakeup(&comport[port].chardev_info);
  if(cdbuf_read(comport[port].txbuf, &data, 1) == 1)
    out8(base+DATA, data);
}

void serial_isr_common(int port) {
  u16 base = comport[port].base;
  u8 reason = in8(base+INT_FIFO_CTRL);
  u8 data;
  if((reason & 0x1) == 0) {
    switch((reason & 0x6)>>1) {
    case 1:
      //送信準備完了
      if(CDBUF_IS_EMPTY(comport[port].txbuf))
        in8(base+INT_FIFO_CTRL);
      else
        serial_send(port);
      break;
    case 2:
    case 6:
      //受信
      data = in8(base+DATA);
      if(CDBUF_IS_EMPTY(comport[port].rxbuf))
        task_wakeup(&comport[port].chardev_info);
      if(!CDBUF_IS_FULL(comport[port].rxbuf))
        cdbuf_write(comport[port].rxbuf, &data, 1);
      break;
    case 3:
      in8(base+LINESTAT);
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


static void serial_open(struct chardev *dev UNUSED) {
  return;
}

static void serial_close(struct chardev *dev UNUSED) {
  return;
}

static u32 serial_read(struct chardev *dev, u8 *dest, u32 count) {
  struct comport *com = container_of(dev, struct comport, chardev_info);
  u32 n = cdbuf_read(com->rxbuf, dest, count);
  return n;
}

static u32 serial_write(struct chardev *dev, u8 *src, u32 count) {
  struct comport *com = container_of(dev, struct comport, chardev_info);
  u32 n = cdbuf_write(com->txbuf, src, count);
  serial_send(com->port);
  return n;
}

