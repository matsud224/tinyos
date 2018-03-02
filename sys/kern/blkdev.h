#pragma once
#include <kern/kernlib.h>
#include <kern/lock.h>

#define BLOCKSIZE 512

struct blkdev_ops {
  void (*open)(int minor);
  void (*close)(int minor);
  void (*readreq)(struct blkbuf *buf);
  void (*writereq)(struct blkbuf *buf);
};

enum {
  BB_ABSENT		= 0x1,
  BB_DIRTY		= 0x2,
  BB_PENDING	= 0x4,
  BB_ERROR		= 0x8,
};

struct blkbuf {
  u16 ref;
  devno_t devno;
  blkno_t blkno;
  u8 *addr;
  u32 flags;
  struct list_head avail_link;
  struct list_head dev_link;
};

void blkdev_init(void);
void blkdev_add(struct blkdev *dev);
int blkdev_register(struct blkdev_ops *ops);
int blkdev_open(devno_t devno);
int blkdev_close(devno_t devno);
struct blkbuf *blkbuf_get(devno_t devno, blkno_t blkno);
void blkbuf_release(struct blkbuf *buf);
void blkbuf_markdirty(struct blkbuf *buf);
int blkdev_wait(struct blkbuf *buf);
void blkbuf_sync(struct blkbuf *buf);
