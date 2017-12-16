#pragma once
#include <kern/kernlib.h>
#include <kern/netdev.h>
#include <kern/pktbuf.h>
#include <kern/queue.h>
#include <net/socket/socket.h>

struct netdev;

struct netdev_ops {
  void (*open)(struct netdev *dev);
  void (*close)(struct netdev *dev);
  int (*tx)(struct netdev *dev, struct pktbuf *pkt);
  struct pktbuf *(*rx)(struct netdev *dev);
};

struct ifaddr {
  struct list_head dev_link;
  struct list_head family_link;
  struct netdev *dev;
  u8 len;
  u8 family;
  u8 addr[];
};

struct netdev {
  u16 devno;
  const struct netdev_ops *ops;
  struct list_head ifaddr_list;
};

extern struct netdev *netdev_tbl[MAX_NETDEV];
extern struct list_head ifaddr_tbl[MAX_PF];

void netdev_init(void);
void netdev_add(struct netdev *dev);
int netdev_tx(struct netdev *dev, struct pktbuf *pkt);
int netdev_tx_nowait(struct netdev *dev, struct pktbuf *pkt);
struct pktbuf *netdev_rx(struct netdev *dev);
struct pktbuf *netdev_rx_nowait(struct netdev *dev);
void netdev_add_ifaddr(struct netdev *dev, struct ifaddr *addr);
struct ifaddr *netdev_find_addr(struct netdev *dev, u16 pf);
