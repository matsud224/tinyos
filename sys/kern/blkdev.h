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
  BB_ABSENT		= 1,
  BB_DIRTY		= 2,
};

enum blkbuf_state {
  BB_DONE			= 0,
  BB_PENDING	= 1,
  BB_ERROR		= 2,
};

struct blkbuf {
  u32 ref;
  devno_t devno;
  blkno_t blkno;
  void *addr;
  u32 flags;
  u32 state;
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
