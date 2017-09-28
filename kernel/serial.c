#include "serial.h"
#include "kernasm.h"
#include "idt.h"
#include "kernlib.h"

#define COMPORT_NUM 2

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


const uint16_t COM_BASE[COMPORT_NUM] = {
  0x3f8, 0x2f8
};


void serial_init() {
  for(int i=0; i<COMPORT_NUM; i++) {
    uint16_t base = COM_BASE[i];
    out8(base + COM_INTENABLE,			0x00);
    out8(base + COM_LINECTRL,				0x80);
    out8(base + COM_DIVISOR_LO,			0x03);
    out8(base + COM_DIVISOR_HI, 		0x00);
    out8(base + COM_LINECTRL,				0x03);
    out8(base + COM_INT_FIFO_CTRL,	0xc7);
    out8(base + COM_MODEMCTRL,			0x0b);
  }
}


