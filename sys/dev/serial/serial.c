#include <kern/pic.h>
#include <kern/chardev.h>
#include <kern/kernasm.h>
#include <kern/idt.h>
#include <kern/kernlib.h>
#include <kern/thread.h>

#define COMPORT_NUM 2

#define SERIAL_BUFSIZE 128

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

static int serial_open(int minor);
static int serial_close(int minor);
static u32 serial_read(int minor, char *dest, size_t count);
static u32 serial_write(int minor, const char *src, size_t count);

static const struct chardev_ops serial_chardev_ops = {
  .open = serial_open,
  .close = serial_close,
  .read = serial_read,
  .write = serial_write,
};

static struct comport{
  u8 port;
  u16 base;
  u8 irq;
  u8 intvec;
  void (*inthandler)(void);
  struct chardev_buf *rxbuf;
  struct chardev_buf *txbuf;
} comport[COMPORT_NUM] = {
  {.port = 0, .base = 0x3f8, .irq = 4, .intvec = IRQ_TO_INTVEC(4), .inthandler = com1_inthandler},
  {.port = 1, .base = 0x2f8, .irq = 3, .intvec = IRQ_TO_INTVEC(3), .inthandler = com2_inthandler}
};

static int SERIAL_MAJOR;

DRIVER_INIT void serial_init() {
  SERIAL_MAJOR = chardev_register(&serial_chardev_ops);
  if(SERIAL_MAJOR < 0) {
    puts("serial: failed to register");
    return;
  }

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

    printf("serial: devno=0x%x\n", DEVNO(SERIAL_MAJOR, i));
  }
}

static void serial_send(int port) {
  u16 base = comport[port].base;
  char data;
  if(CDBUF_IS_FULL(comport[port].txbuf))
    thread_wakeup(&serial_chardev_ops);
  if(cdbuf_read(comport[port].txbuf, &data, 1) == 1)
    out8(base+DATA, data);
}

void serial_isr_common(int port) {
  u16 base = comport[port].base;
  u8 reason = in8(base+INT_FIFO_CTRL);
  char data;
  if((reason & 0x1) == 0) {
    switch((reason & 0x6)>>1) {
    case 1:
      //送信準備完了
      if(CDBUF_IS_EMPTY(comport[port].txbuf))
        in8(base+INT_FIFO_CTRL);
      else
        serial_send(port);
      thread_wakeup(&serial_chardev_ops);
      break;
    case 2:
    case 6:
      //受信
      data = in8(base+DATA);

      if(CDBUF_IS_EMPTY(comport[port].rxbuf))
        thread_wakeup(&serial_chardev_ops);

      if(data == '\r')
        data = '\n';


      if(!CDBUF_IS_FULL(comport[port].rxbuf)) {
        if(data == 0x7f) {
          cdbuf_write(comport[port].rxbuf, "\b \b", 3);
        } else {
          cdbuf_write(comport[port].rxbuf, &data, 1);
        }
      }
      break;
    case 3:
      in8(base+LINESTAT);
      break;
    }
  }

  pic_sendeoi(comport[port].irq);
  thread_yield();
}

void com1_isr() {
  serial_isr_common(0);
}

void com2_isr() {
  serial_isr_common(1);
}

static int serial_check_minor(int minor) {
  if(minor < 0 || minor >= COMPORT_NUM)
    return -1;
  else
    return 0;
}

static int serial_open(int minor) {
  if(serial_check_minor(minor))
    return -1;
  return 0;
}

static int serial_close(int minor) {
  if(serial_check_minor(minor))
    return -1;
  return 0;
}

static u32 serial_read(int minor, char *dest, size_t count) {
  if(serial_check_minor(minor))
    return -1;

  struct comport *com = &comport[minor];
  u32 n = cdbuf_read(com->rxbuf, dest, count);
  //echo back
  cdbuf_write(com->txbuf, dest, n);
  serial_send(com->port);
  return n;
}

static u32 serial_write(int minor, const char *src, size_t count) {
  if(serial_check_minor(minor))
    return -1;

  struct comport *com = &comport[minor];
  u32 n = cdbuf_write(com->txbuf, src, count);
  serial_send(com->port);
  return n;
}

