#pragma once
#include <kern/netdev.h>
#include <kern/types.h>
#include <kern/workqueue.h>

#define ETHER_ADDR_LEN 6

struct etheraddr {
  u8 addr[ETHER_ADDR_LEN];
};

extern const struct etheraddr ETHER_ADDR_BROADCAST;
extern struct workqueue *ether_wq;

void ether_rx(devno_t devno);
void ether_tx(struct pktbuf *frm, struct etheraddr dest, u16 proto, devno_t devno);
