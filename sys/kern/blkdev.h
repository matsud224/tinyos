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
  BB_ERROR		= 0x1,
  BB_DIRTY		= 0x2,
};

enum blkbuf_state {
  BB_ABSENT		= 0,
  BB_PENDING	= 1,
  BB_DONE			= 2,
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
int blkbuf_read_async(struct blkbuf *buf);
int blkbuf_read(struct blkbuf *buf);
int blkbuf_readahead(struct blkbuf *buf, blkno_t ablk);
int blkbuf_write_async(struct blkbuf *buf);
int blkbuf_write(struct blkbuf *buf);
int blkdev_sync(devno_t devno);
int blkdev_sync_all(void);
int blkbuf_flush(struct blkbuf *buf);
void blkbuf_iodone(struct blkbuf *buf);
void blkbuf_readerror(struct blkbuf *buf);
void blkbuf_writeerror(struct blkbuf *buf);
