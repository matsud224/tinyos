#include "netdev.h"

struct netdev *netdev_tbl[MAX_NETDEV];
static u16 nnetdev;

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

struct netdev_queue *ndqueue_create(u8 *mem, size_t count) {
  struct netdev_queue *buf = malloc(sizeof(struct netdev_queue));
  buf->count = count;
  buf->free = count;
  buf->head = 0;
  buf->tail = 0;
  buf->addr = mem;
  return buf;
}

struct pktbuf_head *ndqueue_dequeue(struct netdev_queue *q) {
  struct pktbuf_head *pkt = NULL;
  if(q->head != q->tail) {
    pkt = q->addr[q->tail++];
    if(q->tail == q->count)
      q->tail = 0;
    q->free++;
  }
  return pkt;
}

int ndqueue_enqueue(struct netdev_queue *q, struct pktbuf_head *pkt) {
  if(q->free == 0)
    return 0;
  q->addr[q->head++] = pkt;
  q->free--;
  if(q->head == q->count)
    q->head = 0;
  return 1;
}

int netdev_tx(devno_t devno, struct pktbuf_head *pkt) {
  struct netdev *dev = netdev_tbl[devno];
  while(1) {
    cli();
    int res = dev->ops->tx(dev, pkt);
    if(res < 0)
      task_sleep(dev);
    sti();
  }
  return 0;
}

int netdev_tx_nowait(devno_t devno, struct pktbuf_head *pkt) {
  struct netdev *dev = netdev_tbl[devno];
  cli();
  int res = dev->ops->tx(dev, pkt);
  sti();
  return res;
}

struct pktbuf_head *netdev_rx(devno_t devno) {
  struct netdev *dev = netdev_tbl[devno];
  struct pktbuf_head *pkt = NULL;
  while(1) {
    cli();
    pkt = dev->ops->rx(dev);
    if(pkt == NULL)
      task_sleep(dev);
    sti();
  }
  return pkt;
}

struct pktbuf_head *netdev_rx_nowait(devno_t devno) {
  struct netdev *dev = netdev_tbl[devno];
  cli();
  struct pktbuf_head *pkt = dev->ops->rx(dev);
  sti();
  return pkt;
}

