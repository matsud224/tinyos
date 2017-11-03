#pragma once
#include <net/inet/protohdr.h>

void arp_rx(struct pktbuf *frm, struct ether_arp *earp);
void arp_tx(struct pktbuf *packet, in_addr_t dstaddr, u16 proto);
