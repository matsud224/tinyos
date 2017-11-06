#pragma once
#include <net/inet/protohdr.h>
#include <net/ether/protohdr.h>
#include <kern/netdev.h>

void arp_rx(struct pktbuf *frm);
void arp_tx(struct pktbuf *packet, in_addr_t dstaddr, u16 proto, struct netdev *dev);
