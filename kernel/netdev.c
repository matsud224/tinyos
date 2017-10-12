#include "page.h"
#include "malloc.h"
#include "netdev.h"
#include <stdint.h>
#include <stddef.h>

struct netdev *netdev_tbl[MAX_NETDEV];
static uint16_t nnetdev;

void netdev_init() {
  for(int i=0; i<MAX_BLKDEV; i++)
    netdev_tbl[i] = NULL;
  nnetdev = 0;
}

void netdev_add(struct netdev *dev) {
  netdev_tbl[nnetdev] = dev;
  dev->devno = nnetdev;
  nnetdev++;
}

struct netdev_buf *ndqueue_create(uint32_t *mem, uint32_t size) {
  struct netdev_buf *buf = malloc(sizeof(struct netdev_buf));
  buf->size = size/4;
  buf->free = size/4;
  buf->head = 0;
  buf->tail = 0;
  buf->addr = mem;
  return buf;
}

void *ndqueue_pop(struct netdev_queue *q) {
  void *data = NULL;
  if(buf->head != buf->tail) {
    data = buf->addr[buf->tail++];
    if(buf->tail == buf->size)
      buf->tail = 0;
    buf->free++;
  }
  return data;
}

int ndqueue_push(struct netdev_queue *q, void *data) {
  if(q->free == 0)
    return 0;
  q->addr[q->head++] = data;
  q->free--;
  if(q->head == q->size)
    q->head = 0;
  return 1;
}


uint32_t netdev_rx(uint16_t devno, uint8_t *buf, uint32_t size) {
  struct netdev *dev = netdev_tbl[devno];
  uint32_t remain = count;
  while(remain > 0) {
    cli();
    uint32_t n = dev->ops->rx(dev, dest, remain);
    remain -= n;
    dest += n;
    if(remain > 0)
      task_sleep(dev);
    sti();
  }
  return count;
}

uint32_t netdev_tx(uint16_t devno, uint8_t *buf, uint32_t size) {
  struct netdev *dev = netdev_tbl[devno];
  uint32_t remain = count;
  while(remain > 0) {
    cli();
    uint32_t n = dev->ops->tx(dev, src, count);
    remain -= n;
    src += n;
    if(remain > 0)
      task_sleep(dev);
    sti();
  }
  return count;
}

