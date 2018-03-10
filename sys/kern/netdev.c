#include <kern/netdev.h>
#include <kern/thread.h>

const struct netdev_ops *netdev_tbl[MAX_NETDEV];
struct list_head ifaddr_list[MAX_NETDEV];

struct list_head ifaddr_tbl[MAX_PF];
static u16 nnetdev;

void netdev_init() {
  for(int i=0; i<MAX_NETDEV; i++) {
    netdev_tbl[i] = NULL;
    list_init(&ifaddr_list[i]);
  }

  for(int i=0; i<MAX_PF; i++)
    list_init(&ifaddr_tbl[i]);

  nnetdev = 0;
}

int netdev_register(const struct netdev_ops *ops) {
  if(nnetdev >= MAX_NETDEV)
    return -1;
  netdev_tbl[nnetdev] = ops;
  return nnetdev++;
}

int netdev_tx(devno_t devno, struct pktbuf *pkt) {
  int res = -1;
  const struct netdev_ops *dev = netdev_tbl[DEV_MAJOR(devno)];
IRQ_DISABLE
  while(res < 0) {
    res = dev->tx(DEV_MINOR(devno), pkt);
    if(res < 0)
      thread_sleep(dev);
  }
IRQ_RESTORE
  return 0;
}

int netdev_tx_nowait(devno_t devno, struct pktbuf *pkt) {
  int res = netdev_tbl[DEV_MAJOR(devno)]->tx(DEV_MINOR(devno), pkt);
  return res;
}

struct pktbuf *netdev_rx(devno_t devno) {
  struct pktbuf *pkt = NULL;
  const struct netdev_ops *dev = netdev_tbl[DEV_MAJOR(devno)];
IRQ_DISABLE
  while(pkt == NULL) {
    pkt = dev->rx(DEV_MINOR(devno));
    if(pkt == NULL)
      thread_sleep(dev);
  }
IRQ_RESTORE
  return pkt;
}

struct pktbuf *netdev_rx_nowait(devno_t devno) {
  struct pktbuf *pkt = netdev_tbl[DEV_MAJOR(devno)]->rx(DEV_MINOR(devno));
  return pkt;
}

void netdev_add_ifaddr(devno_t devno, struct ifaddr *addr) {
  addr->devno = devno;
  list_pushback(&addr->dev_link, &ifaddr_list[DEV_MAJOR(devno)]);
  list_pushback(&addr->family_link, &ifaddr_tbl[addr->family]);
}

struct ifaddr *netdev_find_addr(devno_t devno, u16 pf) {
  struct list_head *p;
  list_foreach(p, &ifaddr_list[DEV_MAJOR(devno)]) {
    struct ifaddr *addr = list_entry(p, struct ifaddr, dev_link);
    if(addr->devno == devno && addr->family == pf)
      return addr;
  }
  return NULL;
}


