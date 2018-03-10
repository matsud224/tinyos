#pragma once
#include <kern/kernlib.h>
#include <kern/netdev.h>
#include <kern/pktbuf.h>
#include <kern/queue.h>
#include <net/socket/socket.h>

struct netdev_ops {
  int (*open)(int minor);
  int (*close)(int minor);
  int (*tx)(int minor, struct pktbuf *pkt);
  struct pktbuf *(*rx)(int minor);
};

struct ifaddr {
  struct list_head dev_link;
  struct list_head family_link;
  devno_t devno;
  size_t len;
  u8 family;
  u8 addr[];
};

extern struct list_head ifaddr_tbl[MAX_PF];

void netdev_init(void);
int netdev_register(const struct netdev_ops *ops);
int netdev_tx(devno_t devno, struct pktbuf *pkt);
int netdev_tx_nowait(devno_t devno, struct pktbuf *pkt);
struct pktbuf *netdev_rx(devno_t devno);
struct pktbuf *netdev_rx_nowait(devno_t devno);
void netdev_add_ifaddr(devno_t devno, struct ifaddr *addr);
struct ifaddr *netdev_find_addr(devno_t devno, u16 pf);
