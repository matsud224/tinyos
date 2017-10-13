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

struct netdev_buf *ndqueue_create(u32 *mem, size_t count) {
  struct netdev_buf *buf = malloc(sizeof(struct netdev_buf));
  buf->count = count;
  buf->free = count;
  buf->head = 0;
  buf->tail = 0;
  buf->addr = mem;
  return buf;
}

struct pktbuf *ndqueue_dequeue(struct netdev_queue *q) {
  struct pktbuf *pkt = NULL;
  if(buf->head != buf->tail) {
    pkt = buf->addr[buf->tail++];
    if(buf->tail == buf->count)
      buf->tail = 0;
    buf->free++;
  }
  return pkt;
}

int ndqueue_enqueue(struct netdev_queue *q, struct pktbuf *pkt) {
  if(q->free == 0)
    return 0;
  q->addr[q->head++] = pkt;
  q->free--;
  if(q->head == q->count)
    q->head = 0;
  return 1;
}

int netdev_tx(devno_t devno, struct pktbuf *pkt) {
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

int netdev_tx_nowait(devno_t devno, struct pktbuf *pkt) {
  struct netdev *dev = netdev_tbl[devno];
  cli();
  int res = dev->ops->tx(dev, pkt);
  sti();
  return res;
}

struct pktbuf *netdev_rx(devno_t devno) {
  struct netdev *dev = netdev_tbl[devno];
  while(1) {
    cli();
    struct pktbuf *pkt = dev->ops->rx(dev);
    if(pkt == NULL)
      task_sleep(dev);
    sti();
  }
  return pkt;
}

struct pktbuf *netdev_rx_nowait(devno_t devno) {
  struct netdev *dev = netdev_tbl[devno];
  cli();
  struct pktbuf *pkt = dev->ops->rx(dev);
  sti();
  return pkt;
}

