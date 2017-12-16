#include <kern/netdev.h>
#include <kern/thread.h>

struct netdev *netdev_tbl[MAX_NETDEV];
struct list_head ifaddr_tbl[MAX_PF];
static u16 nnetdev;

void netdev_init() {
  for(int i=0; i<MAX_NETDEV; i++)
    netdev_tbl[i] = NULL;
  for(int i=0; i<MAX_PF; i++)
    list_init(&ifaddr_tbl[i]);
  nnetdev = 0;
}

void netdev_add(struct netdev *dev) {
  netdev_tbl[nnetdev] = dev;
  dev->devno = nnetdev;
  nnetdev++;
}

int netdev_tx(struct netdev *dev, struct pktbuf *pkt) {
  int res = -1;
  while(res < 0) {
    res = dev->ops->tx(dev, pkt);
    if(res < 0)
      thread_sleep(dev);
  }
  return 0;
}

int netdev_tx_nowait(struct netdev *dev, struct pktbuf *pkt) {
  int res = dev->ops->tx(dev, pkt);
  return res;
}

struct pktbuf *netdev_rx(struct netdev *dev) {
  struct pktbuf *pkt = NULL;
  while(pkt == NULL) {
    pkt = dev->ops->rx(dev);
    if(pkt == NULL)
      thread_sleep(dev);
  }
  return pkt;
}

struct pktbuf *netdev_rx_nowait(struct netdev *dev) {
  struct pktbuf *pkt = dev->ops->rx(dev);
  return pkt;
}

void netdev_add_ifaddr(struct netdev *dev, struct ifaddr *addr) {
  addr->dev = dev;
  list_pushback(&addr->dev_link, &dev->ifaddr_list);
  list_pushback(&addr->family_link, &ifaddr_tbl[addr->family]);
}

struct ifaddr *netdev_find_addr(struct netdev *dev, u16 pf) {
  struct list_head *p;
  list_foreach(p, &dev->ifaddr_list) {
    struct ifaddr *addr = list_entry(p, struct ifaddr, dev_link);
    if(addr->family == pf)
      return addr;
  }
  return NULL;
}


