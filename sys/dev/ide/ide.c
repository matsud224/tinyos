#include <kern/pci.h>
#include <kern/idt.h>
#include <kern/pic.h>
#include <kern/page.h>
#include <kern/malloc.h>
#include <kern/kernasm.h>
#include <kern/params.h>
#include <kern/blkdev.h>
#include <kern/list.h>
#include <kern/thread.h>
#include <stdint.h>
#include <stddef.h>

enum sr {
  SR_BSY     = 0x80,    // Busy
  SR_DRDY    = 0x40,    // Drive ready
  SR_DF      = 0x20,    // Drive write fault
  SR_DSC     = 0x10,    // Drive seek complete
  SR_DRQ     = 0x08,    // Drive request ready
  SR_CORR    = 0x04,    // Corrected dide
  SR_IDX     = 0x02,    // Index
  SR_ERR     = 0x01,    // Error
};

enum er {
  ER_BBK      = 0x80,    // Bad sector
  ER_UNC      = 0x40,    // Uncorrectable dide
  ER_MC       = 0x20,    // No media
  ER_IDNF     = 0x10,    // ID mark not found
  ER_MCR      = 0x08,    // No media
  ER_ABRT     = 0x04,    // Command aborted
  ER_TK0NF    = 0x02,    // Track 0 not found
  ER_AMNF     = 0x01,    // No address mark
};

enum atacmd {
  ATACMD_READ_PIO          = 0x20,
  ATACMD_READ_PIO_EXT      = 0x24,
  ATACMD_READ_DMA          = 0xC8,
  ATACMD_READ_DMA_EXT      = 0x25,
  ATACMD_WRITE_PIO         = 0x30,
  ATACMD_WRITE_PIO_EXT     = 0x34,
  ATACMD_WRITE_DMA         = 0xCA,
  ATACMD_WRITE_DMA_EXT     = 0x35,
  ATACMD_CACHE_FLUSH       = 0xE7,
  ATACMD_CACHE_FLUSH_EXT   = 0xEA,
  ATACMD_PACKET            = 0xA0,
  ATACMD_IDENTIFY_PACKET   = 0xA1,
  ATACMD_IDENTIFY          = 0xEC,
};

enum ata_ident {
  IDENT_DEVICETYPE   = 0,
  IDENT_CYLINDERS    = 2,
  IDENT_HEADS        = 6,
  IDENT_SECTORS      = 12,
  IDENT_SERIAL       = 20,
  IDENT_MODEL        = 54,
  IDENT_CAPABILITIES = 98,
  IDENT_FIELDVALID   = 106,
  IDENT_MAX_LBA      = 120,
  IDENT_COMMANDSETS  = 164,
  IDENT_MAX_LBA_EXT  = 200,
};

enum ide_type{
  IDE_ATA        = 0x00,
  IDE_ATAPI      = 0x01,
};

enum master_slave{
  IDE_MASTER     = 0x00,
  IDE_SLAVE      = 0x01,
};

enum regs {
  DATA       = 0x00,
  ERROR      = 0x01,
  FEATURES   = 0x01,
  SECCOUNT0  = 0x02,
  LBA0       = 0x03,
  LBA1       = 0x04,
  LBA2       = 0x05,
  HDDEVSEL   = 0x06,
  COMMAND    = 0x07,
  STATUS     = 0x07,
  SECCOUNT1  = 0x08,
  LBA3       = 0x09,
  LBA4       = 0x0A,
  LBA5       = 0x0B,
  CONTROL    = 0x206,
  ALTSTATUS  = 0x206,
};

enum primary_secondary {
  IDE_PRIMARY      = 0x00,
  IDE_SECONDARY    = 0x01,
};
 
enum ata_direction {
  ATA_READ      = 0x00,
  ATA_WRITE     = 0x01,
};

enum ide_base {
	IDE_PRIMARY_BASE			= 0x1f0,
	IDE_SECONDARY_BASE		= 0x170,
};

enum ide_irq {
  IDE_PRIMARY_IRQ				= 14,
  IDE_SECONDARY_IRQ			= 15,
};

enum bmide_regs {
  BMIDE_PRIMARY_COMMAND				= 0x00,
  BMIDE_PRIMARY_STATUS				= 0x02,
  BMIDE_PRIMARY_PRDTADDR			= 0x04,
  BMIDE_SECONDARY_COMMAND			= 0x08,
  BMIDE_SECONDARY_STATUS			= 0x0a,
  BMIDE_SECONDARY_PRDTADDR		= 0x0c,
};


enum bmide_flags {
  BMIDE_CMD_RW					= 0x08,
  BMIDE_CMD_OP					= 0x01,
  BMIDE_STATUS_SIMPLEX	= 0x80,
  BMIDE_STATUS_D1CAP		= 0x40,
  BMIDE_STATUS_D0CAP		= 0x20,
  BMIDE_STATUS_INT			= 0x4,
  BMIDE_STATUS_ERROR		= 0x2,
  BMIDE_STATUS_ACTIVE		= 0x1,
};

enum flags {
  EOT 	= 0x80000000,
  SRST 	= 0x4,
  HOB 	= 0x80,
  NIEN 	= 0x2,
};

void ide1_isr(void);
void ide2_isr(void);
void ide1_inthandler(void);
void ide2_inthandler(void);

struct {
  u16 base;
  u16 bmide;
  u8 nien;
  u8 irq;
  u8 intvec;
  struct prd *prdt;
  void (*inthandler)(void);
  struct list_head req_queue;
} ide_channel[2] = {
  {.base = IDE_PRIMARY_BASE, .nien = 1, .intvec = IRQ_TO_INTVEC(IDE_PRIMARY_IRQ),
   .inthandler = ide1_inthandler, .irq = IDE_PRIMARY_IRQ},
  {.base = IDE_SECONDARY_BASE, .nien = 1, .intvec = IRQ_TO_INTVEC(IDE_SECONDARY_IRQ),
   .inthandler = ide2_inthandler, .irq = IDE_SECONDARY_IRQ},
};

struct ide_dev{
  u8 exist;
  u8 channel;
  u8 drive;
  u16 type;
  u16 signature;
  u16 capabilities;
  u32 cmdsets;
  u32 size;
  char model[41];
} ide_dev[4];

static void ide_open(int minor);
static void ide_close(int minor);
static void ide_readblk(struct blkbuf *buf);
static void ide_writeblk(struct blkbuf *buf);

struct blkdev_ops ide_blkdev_ops = {
  .open = ide_open,
  .close = ide_close,
  .read = ide_readblk,
  .write = ide_writeblk,
};

struct request {
  struct blkbuf *buf;
  u8 dir;
  u8 nsect;
  u8 rem_nsect;
  u8 *next_addr;
  struct list_head link;
};

static u8 ide_buf[2048];

static void ide_out8(u8 chan, u16 reg, u8 data) {
  if(reg > 0x07 && reg < 0x0c) {
    ide_out8(chan, CONTROL, HOB | ide_channel[chan].nien);
    out8(ide_channel[chan].base + reg - 0x06, data);
    ide_out8(chan, CONTROL, ide_channel[chan].nien);
  } else {
    out8(ide_channel[chan].base + reg, data);
  }
}

static u8 ide_in8(u8 chan, u16 reg) {
  if(reg > 0x07 && reg < 0x0c) {
    ide_out8(chan, CONTROL, HOB | ide_channel[chan].nien);
    u8 r = in8(ide_channel[chan].base + reg - 0x06);
    ide_out8(chan, CONTROL, ide_channel[chan].nien);
    return r;
  } else {
    return in8(ide_channel[chan].base + reg);
  }
}

static void ide_setnien(u8 chan) {
  ide_channel[chan].nien = NIEN;
  ide_out8(chan, CONTROL, ide_channel[chan].nien);
}

static void ide_clrnien(u8 chan) {
  ide_channel[chan].nien = 0;
  ide_out8(chan, CONTROL, ide_channel[chan].nien);
}

static void ide_in_idspace(u8 chan, u8 *buf, int bytes) {
  for(int i = 0; i < bytes; i+=2) {
    u16 data = in16(ide_channel[chan].base + DATA);
    buf[i] = data&0xff;
    buf[i+1] = data>>8;
  }
}

static void wait400ns(u8 chan) {
  for(int i = 0; i < 5; i++)
    ide_in8(chan, ALTSTATUS);
}

static void ide_wait(u8 chan, u8 flag, u8 cond) {
  wait400ns(chan);
  if(cond)
    while((ide_in8(chan, STATUS) & flag) == 0);
  else
    while((ide_in8(chan, STATUS) & flag) != 0);
}

static void ide_drivesel(u8 chan, u8 drv) {
  ide_wait(chan, SR_BSY, 0);
  ide_wait(chan, SR_DRQ, 0);
  ide_out8(chan, HDDEVSEL, 0xa0 | (drv<<4));
  ide_wait(chan, SR_BSY, 0);
  ide_wait(chan, SR_DRQ, 0);
}

static void ide_sendcmd(u8 chan, u8 cmd) {
  ide_out8(chan, COMMAND, cmd);
}

static void ide_channel_init(u8 chan) {
  list_init(&ide_channel[chan].req_queue);
  ide_setnien(chan);
}

DRIVER_INIT void ide_init() {
  int drvno = -1;
  for(int chan = IDE_PRIMARY; chan <= IDE_SECONDARY; chan++) {
    ide_channel_init(chan);

    for(int drv = 0; drv < 2; drv++) {
      drvno++;
      ide_dev[drvno].exist = 0;
      // select drive
      ide_drivesel(chan, drv);
      // send identify command
      ide_out8(chan, SECCOUNT0, 0);
      ide_out8(chan, LBA0, 0);
      ide_out8(chan, LBA1, 0);
      ide_out8(chan, LBA2, 0);
      ide_sendcmd(chan, ATACMD_IDENTIFY);
      wait400ns(chan);
      if(ide_in8(chan, STATUS) & SR_ERR)
        continue;

      if(ide_in8(chan, STATUS) == 0x00)
        continue;

      ide_wait(chan, SR_BSY, 0);
      ide_wait(chan, SR_DRQ, 1);
      u8 cl = ide_in8(chan, LBA1);
      u8 ch = ide_in8(chan, LBA2);
      // detect drive type
      if(cl == 0x14 && ch == 0xeb)
        continue; //ATAPI is not supported
      ide_in_idspace(chan, ide_buf, 512);

      ide_dev[drvno].exist = 1;
      ide_dev[drvno].channel = chan;
      ide_dev[drvno].drive = drv;
      ide_dev[drvno].signature = *(u16 *)(ide_buf + IDENT_DEVICETYPE);
      ide_dev[drvno].capabilities = *(u16 *)(ide_buf + IDENT_CAPABILITIES);
      ide_dev[drvno].cmdsets = *(u32 *)(ide_buf + IDENT_COMMANDSETS);
      ide_dev[drvno].blkdev_info.ops = &ide_blkdev_ops;
     
      if(ide_dev[drvno].cmdsets & (1<<26))
        ide_dev[drvno].size = *(u32 *)(ide_buf + IDENT_MAX_LBA_EXT);
      else
        ide_dev[drvno].size = *(u32 *)(ide_buf + IDENT_MAX_LBA);

      int k;
      for(k = 0; k < 40; k += 2) {
        ide_dev[drvno].model[k] = ide_buf[IDENT_MODEL + k + 1];
        ide_dev[drvno].model[k+1] = ide_buf[IDENT_MODEL + k];
      }
      ide_dev[drvno].model[k] = 0;
    }
  }

  for(int i = 0; i < 4; i++) {
    if(ide_dev[i].exist) {
      printf("ide drive #%d %dKB %s\n", i, ide_dev[i].size*512, ide_dev[i].model);
    } else {
      printf("ide drive #%d not found\n", i);
    }
  }

  for(int chan = IDE_PRIMARY; chan<=IDE_SECONDARY; chan++) {
    idt_register(ide_channel[chan].intvec, IDT_INTGATE, ide_channel[chan].inthandler);
    pic_clearmask(ide_channel[chan].irq);
  }
}

static int ide_ata_access(u8 dir, u8 drv, u32 lba, u8 nsect) {
  u8 lba_mode, lba_io[6], chan = drv>>1, slave = drv&1;
  u8 head, cmd = 0;

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
    ide_out8(chan, HDDEVSEL, 0xa0 | (slave<<4) | head);
  else
    ide_out8(chan, HDDEVSEL, 0xe0 | (slave<<4) | head);

  ide_wait(chan, SR_BSY, 0);
  ide_wait(chan, SR_DRQ, 0);

  if (lba_mode == 2) {
    ide_out8(chan, SECCOUNT1,   0);
    ide_out8(chan, LBA3,   lba_io[3]);
    ide_out8(chan, LBA4,   lba_io[4]);
    ide_out8(chan, LBA5,   lba_io[5]);
  }
  ide_out8(chan, ERROR,   0);
  ide_out8(chan, SECCOUNT0,   nsect);
  ide_out8(chan, LBA0,   lba_io[0]);
  ide_out8(chan, LBA1,   lba_io[1]);
  ide_out8(chan, LBA2,   lba_io[2]);

  if (lba_mode == 1 && dir == 0)			cmd = ATACMD_READ_PIO;
  else if (lba_mode == 2 && dir == 0)	cmd = ATACMD_READ_PIO_EXT;
  else if (lba_mode == 1 && dir == 1)	cmd = ATACMD_WRITE_PIO;
  else if (lba_mode == 2 && dir == 1)	cmd = ATACMD_WRITE_PIO_EXT;
  ide_wait(chan, SR_BSY, 0);
  ide_wait(chan, SR_DRDY, 1);
  ide_sendcmd(chan, cmd);
  return 0;
}

static void ide_procnext(u8 chan);

void *ide_request(struct request *req) {
  u8 chan = ide_dev[DEV_MINOR(req->buf->devno)]->channel;

  cli();
  list_pushback(&req->link, &ide_channel[chan].req_queue);
  ide_procnext(chan);
  sti();

  return req;
}

static void ide_procnext(u8 chan) {
  if(list_is_empty(&ide_channel[chan].req_queue))
    return;
  struct request *req = container_of(ide_channel[chan].req_queue.next, struct request, link);

  struct ide_dev *dev = ide_dev[DEV_MINOR(req->buf->devno)];
  ide_ata_access(req->dir, (dev->channel<<1)|dev->drive, req->blockno, req->nsect);
}

static void dequeue_and_next(u8 chan) {
  struct list_head *head = list_pop(&ide_channel[chan].req_queue);
  if(head) {
    struct request *req = container_of(head, struct request, link);
    thread_wakeup(req->buf);
  }
  ide_procnext(chan);
}

static void ide_isr_common(u8 chan) {
  struct request *req = container_of(ide_channel[chan].req_queue.next, struct request, link);
  if((ide_in8(chan, STATUS) & SR_ERR) == 0) {
    if(req != NULL) {
      for(int i=0; i<512; i+=2) {
        u16 data;
        data = in16(ide_channel[chan].base + DATA);
        *(req->next_addr++) = data&0xff;
        *(req->next_addr++) = data>>8;
      }
      if(--req->rem_nsect == 0) {
        req->buf->flags &= ~BB_PENDING;
        dequeue_and_next(chan);
      }
    }
  } else {
    req->buf->flags &= ~BB_PENDING;
    req->buf->flags |= BB_ERROR;
    puts("ide error!");
    dequeue_and_next(chan);
  }
  ide_in8(chan, STATUS);
  pic_sendeoi();
  thread_yield();
}

void ide1_isr() {
  ide_isr_common(IDE_PRIMARY);
}

void ide2_isr() {
  ide_isr_common(IDE_SECONDARY);
}

static int check_minor(int minor) {
  if(minor < 0 || minor > 3)
    return -1;
  if(!ide_dev[minor].exist)
    return -1;
  return 0;
}

static int ide_open(int minor) {
  return check_minor(minor);
}

static int ide_close(int minor) {
  return check_minor(minor);
}

static int ide_readblk(struct blkbuf *buf) {
  if(check_minor(minor))
    return -1;
  struct request *req = malloc(sizeof(struct request));
  req->buf = buf;
  req->nsect = req->rem_nsect = 1;
  req->next_addr = buf->addr;
  req->dir = ATA_READ;
  ide_request(req);
  return 0;
}


static int ide_writeblk(struct blkbuf *buf) {
  if(check_minor(minor))
    return -1;
  struct request *req = malloc(sizeof(struct request));
  req->buf = buf;
  req->nsect = req->rem_nsect = 1;
  req->next_addr = buf->addr;
  req->dir = ATA_WRITE;
  ide_request(req);
  return 0;
}


