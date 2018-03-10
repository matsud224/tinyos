#pragma once
#include <kern/kernlib.h>
#include <kern/lock.h>

#define BLOCKSIZE 512

struct blkbuf;

struct blkdev_ops {
  int (*open)(int minor);
  int (*close)(int minor);
  int (*readreq)(struct blkbuf *buf);
  int (*writereq)(struct blkbuf *buf);
};

enum blkbuf_flags {
  BB_ABSENT		= 0x1,
  BB_DIRTY		= 0x2,
  BB_PENDING	= 0x4,
  BB_ERROR		= 0x8,
};

struct blkbuf {
  u32 ref;
  devno_t devno;
  blkno_t blkno;
  void *addr;
  u32 flags;
  struct list_head avail_link;
  struct list_head dev_link;
};

extern const struct file_ops blkdev_file_ops;

void blkdev_init(void);
int blkdev_register(struct blkdev_ops *ops);
int blkdev_open(devno_t devno);
int blkdev_close(devno_t devno);
struct blkbuf *blkbuf_get(devno_t devno, blkno_t blkno);
void blkbuf_release(struct blkbuf *buf);
void blkbuf_markdirty(struct blkbuf *buf);
int blkdev_wait(struct blkbuf *buf);
int blkbuf_sync(struct blkbuf *buf);
