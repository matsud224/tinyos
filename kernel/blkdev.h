#pragma once

#include "params.h"
#include <stdint.h>
#include <stddef.h>

#define BLOCKSIZE 512

#define BDBUF_EMPTY 0x1
#define BDBUF_DIRTY 0x2
#define BDBUF_PENDING 0x4
#define BDBUF_ERROR 0x8
#define BDBUF_READY 0x10

typedef uint32_t blkno_t;
typedef uint16_t devno_t;

struct blkdev_buf {
  uint16_t ref;
  struct blkdev *dev;
  uint32_t blockno;
  uint8_t *addr;
  volatile uint32_t flags;
  struct blkdev_buf *next;
  struct blkdev_buf *prev;
};

struct blkdev_ops {
  void (*open)(void);
  void (*close)(void);
  void (*sync)(struct blkdev_buf *);
};

struct blkdev {
  uint16_t devno;
  struct blkdev_ops *ops;
  struct blkdev_buf *buf_list;
};

extern struct blkdev *blkdev_tbl[MAX_BLKDEV];

void blkdev_init(void);
void blkdev_add(struct blkdev *dev);
struct blkdev_buf *blkdev_getbuf(uint16_t devno, uint64_t blockno);
void blkdev_releasebuf(struct blkdev_buf *buf);
void blkdev_buf_sync_nowait(struct blkdev_buf *buf);
void blkdev_buf_sync(struct blkdev_buf *buf);
void blkdev_buf_markdirty(struct blkdev_buf *buf);



