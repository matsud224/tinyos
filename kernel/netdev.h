#pragma once
#include "kernlib.h"
#include "netdev.h"
#include "pktbuf.h"

#define NDQUEUE_IS_EMPTY(b) ((b)->count == (b)->free)
#define NDQUEUE_IS_FULL(b) ((b)->free == 0)

struct netdev_queue {
  size_t count;
  u32 free;
  u32 head; //次の書き込み位置
  u32 tail; //次の読み出し位置
  struct pktbuf_head **addr;
};

struct netdev;

struct netdev_ops {
  void (*open)(struct netdev *dev);
  void (*close)(struct netdev *dev);
  int (*tx)(struct netdev *dev, struct pktbuf_head *pkt);
  struct pktbuf_head *(*rx)(struct netdev *dev);
};

struct netdev {
  u16 devno;
  const struct netdev_ops *ops;
};

extern struct netdev *netdev_tbl[MAX_NETDEV];

void netdev_init(void);
void netdev_add(struct netdev *dev);
struct netdev_queue *ndqueue_create(u8 *mem, size_t count);
struct pktbuf_head *ndqueue_dequeue(struct netdev_queue *q);
int ndqueue_enqueue(struct netdev_queue *q, struct pktbuf_head *pkt);
int netdev_tx(devno_t devno, struct pktbuf_head *pkt);
int netdev_tx_nowait(devno_t devno, struct pktbuf_head *pkt);
struct pktbuf_head *netdev_rx(devno_t devno);
struct pktbuf_head *netdev_rx_nowait(devno_t devno);

