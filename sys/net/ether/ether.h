#pragma once
#include <kern/netdev.h>
#include <kern/types.h>

#define ETHER_ADDR_LEN 6

struct etheraddr {
  u8 addr[ETHER_ADDR_LEN];
};

extern const struct etheraddr ETHER_ADDR_BROADCAST;

void ether_rx(void *_devno);
void ether_tx(struct pktbuf *frm, struct etheraddr dest, u16 proto, struct netdev *dev);
