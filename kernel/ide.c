#include "ide.h"
#include "vga.h"
#include "pci.h"
#include "idt.h"
#include "pic.h"
#include "page.h"
#include "malloc.h"
#include "kernasm.h"
#include "params.h"
#include "common.h"
#include "blkdev.h"
#include <stdint.h>
#include <stddef.h>

#define ATA_SR_BSY     0x80    // Busy
#define ATA_SR_DRDY    0x40    // Drive ready
#define ATA_SR_DF      0x20    // Drive write fault
#define ATA_SR_DSC     0x10    // Drive seek complete
#define ATA_SR_DRQ     0x08    // Dide request ready
#define ATA_SR_CORR    0x04    // Corrected dide
#define ATA_SR_IDX     0x02    // Inlex
#define ATA_SR_ERR     0x01    // Error

#define ATA_ER_BBK      0x80    // Bad sector
#define ATA_ER_UNC      0x40    // Uncorrectable dide
#define ATA_ER_MC       0x20    // No media
#define ATA_ER_IDNF     0x10    // ID mark not found
#define ATA_ER_MCR      0x08    // No media
#define ATA_ER_ABRT     0x04    // Command aborted
#define ATA_ER_TK0NF    0x02    // Track 0 not found
#define ATA_ER_AMNF     0x01    // No address mark

#define ATA_CMD_READ_PIO          0x20
#define ATA_CMD_READ_PIO_EXT      0x24
#define ATA_CMD_READ_DMA          0xC8
#define ATA_CMD_READ_DMA_EXT      0x25
#define ATA_CMD_WRITE_PIO         0x30
#define ATA_CMD_WRITE_PIO_EXT     0x34
#define ATA_CMD_WRITE_DMA         0xCA
#define ATA_CMD_WRITE_DMA_EXT     0x35
#define ATA_CMD_CACHE_FLUSH       0xE7
#define ATA_CMD_CACHE_FLUSH_EXT   0xEA
#define ATA_CMD_PACKET            0xA0
#define ATA_CMD_IDENTIFY_PACKET   0xA1
#define ATA_CMD_IDENTIFY          0xEC

#define ATA_IDENT_DEVICETYPE   0
#define ATA_IDENT_CYLINDERS    2
#define ATA_IDENT_HEADS        6
#define ATA_IDENT_SECTORS      12
#define ATA_IDENT_SERIAL       20
#define ATA_IDENT_MODEL        54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID   106
#define ATA_IDENT_MAX_LBA      120
#define ATA_IDENT_COMMANDSETS  164
#define ATA_IDENT_MAX_LBA_EXT  200

#define IDE_ATA        0x00
#define IDE_ATAPI      0x01
 
#define ATA_MASTER     0x00
#define ATA_SLAVE      0x01

#define ATA_REG_DATA       0x00
#define ATA_REG_ERROR      0x01
#define ATA_REG_FEATURES   0x01
#define ATA_REG_SECCOUNT0  0x02
#define ATA_REG_LBA0       0x03
#define ATA_REG_LBA1       0x04
#define ATA_REG_LBA2       0x05
#define ATA_REG_HDDEVSEL   0x06
#define ATA_REG_COMMAND    0x07
#define ATA_REG_STATUS     0x07
#define ATA_REG_SECCOUNT1  0x08
#define ATA_REG_LBA3       0x09
#define ATA_REG_LBA4       0x0A
#define ATA_REG_LBA5       0x0B
#define ATA_REG_CONTROL    0x206
#define ATA_REG_ALTSTATUS  0x206

#define IDE_PRIMARY      0x00
#define IDE_SECONDARY    0x01
 
#define ATA_READ      0x00
#define ATA_WRITE     0x01

#define	IDE_PRIMARY_BASE			0x1f0
#define	IDE_SECONDARY_BASE		0x170

#define IDE_PRIMARY_INTVEC					0x2e
#define IDE_SECONDARY_INTVEC				0x2f
#define IDE_PRIMARY_IRQ							14
#define IDE_SECONDARY_IRQ						15

#define BMIDE_PRIMARY_COMMAND				0x00
#define BMIDE_PRIMARY_STATUS				0x02
#define BMIDE_PRIMARY_PRDTADDR			0x04
#define BMIDE_SECONDARY_COMMAND			0x08
#define BMIDE_SECONDARY_STATUS			0x0a
#define BMIDE_SECONDARY_PRDTADDR		0x0c

#define BMIDE_CMD_RW					0x08
#define BMIDE_CMD_OP					0x01
#define BMIDE_STATUS_SIMPLEX	0x80
#define BMIDE_STATUS_D1CAP		0x40
#define BMIDE_STATUS_D0CAP		0x20
#define BMIDE_STATUS_INT			0x4
#define BMIDE_STATUS_ERROR		0x2
#define BMIDE_STATUS_ACTIVE		0x1

#define EOT 0x80000000

#define SRST 0x4
#define HOB 0x80
#define NIEN 0x2


struct {
  uint16_t base;
  uint16_t bmide;
  uint8_t nien;
  uint8_t irq;
  uint8_t intvec;
  struct prd *prdt;
  void (*inthandler)(void);
  struct request *queue_head;
  struct request *queue_tail;
} ide_channel[2];

struct ide_dev{
  uint8_t exist;
  uint8_t channel;
  uint8_t drive;
  uint16_t type;
  uint16_t signature;
  uint16_t capabilities;
  uint32_t cmdsets;
  uint32_t size;
  char model[41];
  struct blkdev blkdev_info;
} ide_dev[4];

static void ide_open(void);
static void ide_close(void);
static void ide_sync(struct blkdev_buf *);
struct blkdev_ops ide_blkdev_ops = {
  .open = ide_open,
  .close = ide_close,
  .sync = ide_sync
};

struct request {
  struct blkdev_buf *buf;
  uint8_t dir;
  uint8_t nsect;
  uint8_t rem_nsect;
  uint8_t *next_addr;
  struct request *next;
};

static uint8_t ide_buf[2048];

static void ide_out8(uint8_t chan, uint16_t reg, uint8_t data) {
  if(reg > 0x07 && reg < 0x0c) {
    ide_out8(chan, ATA_REG_CONTROL, HOB | ide_channel[chan].nien);
    out8(ide_channel[chan].base + reg - 0x06, data);
    ide_out8(chan, ATA_REG_CONTROL, ide_channel[chan].nien);
  } else {
    out8(ide_channel[chan].base + reg, data);
  }
}

static uint8_t ide_in8(uint8_t chan, uint16_t reg) {
  if(reg > 0x07 && reg < 0x0c) {
    ide_out8(chan, ATA_REG_CONTROL, HOB | ide_channel[chan].nien);
    uint8_t r = in8(ide_channel[chan].base + reg - 0x06);
    ide_out8(chan, ATA_REG_CONTROL, ide_channel[chan].nien);
    return r;
  } else {
    return in8(ide_channel[chan].base + reg);
  }
}

static void ide_setnien(uint8_t chan) {
  ide_channel[chan].nien = NIEN;
  ide_out8(chan, ATA_REG_CONTROL, ide_channel[chan].nien);
}

static void ide_clrnien(uint8_t chan) {
  ide_channel[chan].nien = 0;
  ide_out8(chan, ATA_REG_CONTROL, ide_channel[chan].nien);
}

static void ide_in_idspace(uint8_t chan, uint8_t *buf, int bytes) {
  for(int i = 0; i < bytes; i+=2) {
    uint16_t data = in16(ide_channel[chan].base + ATA_REG_DATA);
    buf[i] = data&0xff;
    buf[i+1] = data>>8;
  }
}

static void wait400ns(uint8_t chan) {
  for(int i = 0; i < 5; i++)
    ide_in8(chan, ATA_REG_ALTSTATUS);
}

static void ide_wait(uint8_t chan, uint8_t flag, uint8_t cond) {
  wait400ns(chan);
  if(cond)
    while((ide_in8(chan, ATA_REG_STATUS) & flag) == 0);
  else
    while((ide_in8(chan, ATA_REG_STATUS) & flag) != 0);
}

static void ide_drivesel(uint8_t chan, uint8_t drv) {
  ide_wait(chan, ATA_SR_BSY, 0);
  ide_wait(chan, ATA_SR_DRQ, 0);
  ide_out8(chan, ATA_REG_HDDEVSEL, 0xa0 | (drv<<4));
  ide_wait(chan, ATA_SR_BSY, 0);
  ide_wait(chan, ATA_SR_DRQ, 0);
}

static void ide_sendcmd(uint8_t chan, uint8_t cmd) {
  ide_out8(chan, ATA_REG_COMMAND, cmd);
}

static void ide_channel_init(uint8_t chan) {
  ide_channel[chan].base = chan==IDE_PRIMARY ? IDE_PRIMARY_BASE : IDE_SECONDARY_BASE;
  ide_channel[chan].nien = 1;
  ide_channel[chan].intvec = chan==IDE_PRIMARY ? IDE_PRIMARY_INTVEC : IDE_SECONDARY_INTVEC;
  ide_channel[chan].inthandler = chan==IDE_PRIMARY ? ide1_inthandler : ide2_inthandler;
  ide_channel[chan].irq = chan==IDE_PRIMARY ? IDE_PRIMARY_IRQ : IDE_SECONDARY_IRQ;
  ide_channel[chan].queue_head = NULL;
  ide_channel[chan].queue_tail = NULL;
  ide_setnien(chan);
}

void ide_init() {
  int drvno = -1;
  for(int chan = IDE_PRIMARY; chan <= IDE_SECONDARY; chan++) {
    ide_channel_init(chan);

    for(int drv = 0; drv < 2; drv++) {
      drvno++;
      ide_dev[drvno].exist = 0;
      // select drive
      ide_drivesel(chan, drv);
      // send identify command
      ide_out8(chan, ATA_REG_SECCOUNT0, 0);
      ide_out8(chan, ATA_REG_LBA0, 0);
      ide_out8(chan, ATA_REG_LBA1, 0);
      ide_out8(chan, ATA_REG_LBA2, 0);
      ide_sendcmd(chan, ATA_CMD_IDENTIFY);
      wait400ns(chan);
      if(ide_in8(chan, ATA_REG_STATUS) & ATA_SR_ERR)
        continue;

      if(ide_in8(chan, ATA_REG_STATUS) == 0x00)
        continue;

      ide_wait(chan, ATA_SR_BSY, 0);
      ide_wait(chan, ATA_SR_DRQ, 1);
      uint8_t cl = ide_in8(chan, ATA_REG_LBA1);
      uint8_t ch = ide_in8(chan, ATA_REG_LBA2);
      // detect drive type
      if(cl == 0x14 && ch == 0xeb)
        continue; //ATAPI is not supported
      ide_in_idspace(chan, ide_buf, 512);

      ide_dev[drvno].exist = 1;
      ide_dev[drvno].channel = chan;
      ide_dev[drvno].drive = drv;
      ide_dev[drvno].signature = *(uint16_t *)(ide_buf + ATA_IDENT_DEVICETYPE);
      ide_dev[drvno].capabilities = *(uint16_t *)(ide_buf + ATA_IDENT_CAPABILITIES);
      ide_dev[drvno].cmdsets = *(uint32_t *)(ide_buf + ATA_IDENT_COMMANDSETS);
      ide_dev[drvno].blkdev_info.ops = &ide_blkdev_ops;
      ide_dev[drvno].blkdev_info.buf_list = NULL;
     
      if(ide_dev[drvno].cmdsets & (1<<26))
        ide_dev[drvno].size = *(uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA_EXT);
      else
        ide_dev[drvno].size = *(uint32_t *)(ide_buf + ATA_IDENT_MAX_LBA);

      int k;
      for(k = 0; k < 40; k += 2) {
        ide_dev[drvno].model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
        ide_dev[drvno].model[k+1] = ide_buf[ATA_IDENT_MODEL + k];
      }
      ide_dev[drvno].model[k] = 0;
    }
  }

  for(int i = 0; i < 4; i++) {
    if(ide_dev[i].exist) {
      printf("ide drive #%d %dKB %s\n", i, ide_dev[i].size*512, ide_dev[i].model);
      blkdev_add(&ide_dev[i].blkdev_info);
    } else {
      printf("ide drive #%d not found\n", i);
    }
  }

  for(int chan = IDE_PRIMARY; chan<=IDE_SECONDARY; chan++) {
    idt_register(ide_channel[chan].intvec, IDT_INTGATE, ide_channel[chan].inthandler);
    pic_clearmask(ide_channel[chan].irq);
  }
}

static int ide_ata_access(uint8_t dir, uint8_t drv, uint32_t lba, uint8_t nsect) {
  uint8_t lba_mode, lba_io[6], chan = drv>>1, slave = drv&1;
  uint8_t head, cmd = 0;

  if(!ide_dev[drv].exist)
    return -1;
  if((ide_dev[drv].capabilities & 0x100) == 0)
    puts("DMA is not supported!");

  ide_clrnien(chan);
  if(lba >= 0x10000000) {
    // LBA48
    lba_mode = 2;
    lba_io[0] = (lba & 0x000000FF) >> 0;
    lba_io[1] = (lba & 0x0000FF00) >> 8;
    lba_io[2] = (lba & 0x00FF0000) >> 16;
    lba_io[3] = (lba & 0xFF000000) >> 24;
    lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
    lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
    head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
  } else if(1/*ide_dev[drv].capabilities & 0x200*/) {
    // LBA28
    lba_mode  = 1;
    lba_io[0] = (lba & 0x00000FF) >> 0;
    lba_io[1] = (lba & 0x000FF00) >> 8;
    lba_io[2] = (lba & 0x0FF0000) >> 16;
    lba_io[3] = 0; // These Registers are not used here.
    lba_io[4] = 0; // These Registers are not used here.
    lba_io[5] = 0; // These Registers are not used here.
    head      = (lba & 0xF000000) >> 24;
  } else {
    // LBA is not supported
    return -3;
  }

  ide_drivesel(chan, slave);

  if(lba_mode == 0)
    ide_out8(chan, ATA_REG_HDDEVSEL, 0xa0 | (slave<<4) | head);
  else
    ide_out8(chan, ATA_REG_HDDEVSEL, 0xe0 | (slave<<4) | head);

  ide_wait(chan, ATA_SR_BSY, 0);
  ide_wait(chan, ATA_SR_DRQ, 0);

  if (lba_mode == 2) {
    ide_out8(chan, ATA_REG_SECCOUNT1,   0);
    ide_out8(chan, ATA_REG_LBA3,   lba_io[3]);
    ide_out8(chan, ATA_REG_LBA4,   lba_io[4]);
    ide_out8(chan, ATA_REG_LBA5,   lba_io[5]);
  }
  ide_out8(chan, ATA_REG_ERROR,   0);
  ide_out8(chan, ATA_REG_SECCOUNT0,   nsect);
  ide_out8(chan, ATA_REG_LBA0,   lba_io[0]);
  ide_out8(chan, ATA_REG_LBA1,   lba_io[1]);
  ide_out8(chan, ATA_REG_LBA2,   lba_io[2]);

  if (lba_mode == 1 && dir == 0) cmd = ATA_CMD_READ_PIO;
  else if (lba_mode == 2 && dir == 0) cmd = ATA_CMD_READ_PIO_EXT;
  else if (lba_mode == 1 && dir == 1) cmd = ATA_CMD_WRITE_PIO;
  else if (lba_mode == 2 && dir == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
  ide_wait(chan, ATA_SR_BSY, 0);
  ide_wait(chan, ATA_SR_DRDY, 1);
  ide_sendcmd(chan, cmd);
  return 0;
}

static void ide_procnext(uint8_t chan);

void *ide_request(struct request *req) {
  uint8_t chan = container_of(req->buf->dev, struct ide_dev, blkdev_info)->channel;
  cli();
  if(ide_channel[chan].queue_tail == NULL) {
    ide_channel[chan].queue_head = req;
    ide_channel[chan].queue_tail = req;
  	ide_procnext(chan);
  } else {
    ide_channel[chan].queue_tail->next = req;
    ide_channel[chan].queue_tail = req;
  }
  sti();

  return req;
}

static void ide_procnext(uint8_t chan) {
  // already cli() called
  struct request *req = ide_channel[chan].queue_head;
  if(req == NULL)
    return;

  struct ide_dev *dev = container_of(req->buf->dev, struct ide_dev, blkdev_info);
  ide_ata_access(req->dir, (dev->channel<<1)|dev->drive, req->buf->blockno, req->nsect);
}

static void dequeue_and_next(uint8_t chan) {
  cli();
  struct request *headreq = ide_channel[chan].queue_head;
  if(headreq) {
    headreq->buf->wait = 0;
    ide_channel[chan].queue_head = headreq->next;
    headreq->next = NULL;
    if(ide_channel[chan].queue_head == NULL)
      ide_channel[chan].queue_tail = NULL;
  }
  ide_procnext(chan);
  sti();
}

static void ide_isr_common(uint8_t chan) {
  struct request *req = ide_channel[chan].queue_head;
  if((ide_in8(chan, ATA_REG_STATUS) & ATA_SR_ERR) == 0) {
    if(req != NULL) {
      for(int i=0; i<512; i+=2) {
        uint16_t data;
        data = in16(ide_channel[chan].base + ATA_REG_DATA);
        *(req->next_addr++) = data&0xff;
        *(req->next_addr++) = data>>8;
      }
      if(--req->rem_nsect == 0)
        dequeue_and_next(chan);
    }
  } else {
    req->buf->flags |= BDBUF_ERROR;
    puts("ide error!!!");
    dequeue_and_next(chan);
  }
  ide_in8(chan, ATA_REG_STATUS);
  pic_sendeoi();
}

void ide1_isr() {
  //puts("IDE Primary Interrupt!");
  ide_isr_common(IDE_PRIMARY);
}

void ide2_isr() {
  //puts("IDE Secondary Interrupt!");
  ide_isr_common(IDE_SECONDARY);
}

static void ide_open() {
  return;
}

static void ide_close() {
  return;
}

static void ide_sync(struct blkdev_buf *buf) {
  int dir;
  if(buf->flags & BDBUF_EMPTY)
    dir = ATA_READ;
  else if(buf->flags & BDBUF_DIRTY)
    dir = ATA_WRITE;
  else
    return;

  buf->wait = 1;
  struct request *req = malloc(sizeof(struct request));
  req->buf = buf;
  req->nsect = req->rem_nsect = 1;
  req->next_addr = buf->addr;
  req->dir = dir;
  req->next = NULL;
  ide_request(req);
}


