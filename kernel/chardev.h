#pragma once

#include "params.h"
#include <stdint.h>
#include <stddef.h>

#define CDBUF_IS_EMPTY(b) ((b)->size == (b)->free)
#define CDBUF_IS_FULL(b) ((b)->free == 0)

struct chardev_buf {
  uint32_t size;
  uint32_t free;
  uint32_t head; //次の書き込み位置
  uint32_t tail; //次の読み出し位置
  uint8_t *addr;
};

struct chardev;

struct chardev_ops {
  void (*open)(struct chardev *dev);
  void (*close)(struct chardev *dev);
  uint32_t (*read)(struct chardev *dev, uint8_t *dest, uint32_t count);
  uint32_t (*write)(struct chardev *dev, uint8_t *src, uint32_t count);
};

struct chardev {
  uint16_t devno;
  struct chardev_ops *ops;
};

extern struct chardev *chardev_tbl[MAX_CHARDEV];

void chardev_init(void);
void chardev_add(struct chardev *dev);
struct chardev_buf *cdbuf_create(uint8_t *mem, uint32_t size);
uint32_t cdbuf_read(struct chardev_buf *buf, uint8_t *dest, uint32_t count);
uint32_t cdbuf_write(struct chardev_buf *buf, uint8_t *src, uint32_t count);
uint32_t chardev_read(uint16_t devno, uint8_t *dest, uint32_t count);
uint32_t chardev_write(uint16_t devno, uint8_t *src, uint32_t count);


