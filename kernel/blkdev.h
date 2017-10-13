#pragma once
#include "kernlib.h"

#define BLOCKSIZE 512

enum {
  BDBUF_EMPTY		= 0x1,
  BDBUF_DIRTY		= 0x2,
  BDBUF_PENDING	= 0x4,
  BDBUF_ERROR		= 0x8,
  BDBUF_READY		= 0x10,
};

struct blkdev_buf {
  u16 ref;
  struct blkdev *dev;
  blkno_t blockno;
  u8 *addr;
  u32 flags;
  struct blkdev_buf *next;
  struct blkdev_buf *prev;
};

struct blkdev_ops {
  void (*open)(void);
  void (*close)(void);
  void (*sync)(struct blkdev_buf *);
};

struct blkdev {
  devno_t devno;
  struct blkdev_ops *ops;
  struct blkdev_buf *buf_list;
};

extern struct blkdev *blkdev_tbl[MAX_BLKDEV];

void blkdev_init(void);
void blkdev_add(struct blkdev *dev);
struct blkdev_buf *blkdev_getbuf(devno_t devno, blkno_t blockno);
void blkdev_releasebuf(struct blkdev_buf *buf);
void blkdev_buf_sync_nowait(struct blkdev_buf *buf);
void blkdev_buf_sync(struct blkdev_buf *buf);
void blkdev_buf_markdirty(struct blkdev_buf *buf);



