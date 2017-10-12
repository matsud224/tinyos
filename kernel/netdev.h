#pragma once

#include "params.h"
#include <stdint.h>
#include <stddef.h>

#define NDQUEUE_IS_EMPTY(b) ((b)->size == (b)->free)
#define NDQUEUE_IS_FULL(b) ((b)->free == 0)

struct netdev_queue {
  uint32_t size;
  uint32_t free;
  uint32_t head; //次の書き込み位置
  uint32_t tail; //次の読み出し位置
  void **addr;
};


struct netdev;

struct netdev_ops {
  void (*open)(struct netdev *dev);
  void (*close)(struct netdev *dev);
  uint32_t (*tx)(struct netdev *dev, uint8_t *buf, uint32_t size);
  uint32_t (*rx)(struct netdev *dev, uint8_t *buf, uint32_t size);
};

struct netdev {
  uint16_t devno;
  struct netdev_ops *ops;
};

extern struct netdev *netdev_tbl[MAX_CHARDEV];

void netdev_init(void);
void netdev_add(struct netdev *dev);
struct netdev_buf *ndqueue_create(uint8_t *mem, uint32_t size);
uint32_t ndqueue_read(struct netdev_buf *buf, uint8_t *dest, uint32_t count);
uint32_t ndqueue_write(struct netdev_buf *buf, uint8_t *src, uint32_t count);
uint32_t netdev_tx(uint16_t devno, uint8_t *buf, uint32_t size);
uint32_t netdev_rx(uint16_t devno, uint8_t *buf, uint32_t size);

