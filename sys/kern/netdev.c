#include <kern/netdev.h>
#include <kern/task.h>

struct netdev *netdev_tbl[MAX_NETDEV];
static u16 nnetdev;

void netdev_init() {
  for(int i=0; i<MAX_NETDEV; i++)
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

struct pktbuf *ndqueue_dequeue(struct netdev_queue *q) {
  struct pktbuf *pkt = NULL;
  if(q->head != q->tail) {
    pkt = q->addr[q->tail++];
    if(q->tail == q->count)
      q->tail = 0;
    q->free++;
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

int netdev_tx(struct netdev *dev, struct pktbuf *pkt) {
  int res = -1;
  while(res < 0) {
    cli();
    dev->ops->tx(dev, pkt);
    if(res < 0)
      task_sleep(dev);
    sti();
  }
  return 0;
}

int netdev_tx_nowait(struct netdev *dev, struct pktbuf *pkt) {
  cli();
  int res = dev->ops->tx(dev, pkt);
  sti();
  return res;
}

struct pktbuf *netdev_rx(struct netdev *dev) {
  struct pktbuf *pkt = NULL;
  while(pkt == NULL) {
    cli();
    pkt = dev->ops->rx(dev);
    if(pkt == NULL)
      task_sleep(dev);
    sti();
  }
  return pkt;
}

struct pktbuf *netdev_rx_nowait(struct netdev *dev) {
  cli();
  struct pktbuf *pkt = dev->ops->rx(dev);
  sti();
  return pkt;
}

void netdev_add_ifaddr(struct netdev *dev, struct ifaddr *addr) {
  addr->dev = dev;
  list_pushback(&addr->dev_link, &dev->ifaddr_list);
  list_pushback(&addr->family_link, &ifaddr_table[addr->family]);
}
