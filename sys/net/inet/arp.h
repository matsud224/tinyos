#pragma once
#include <net/inet/protohdr.h>
#include <net/ether/protohdr.h>

void arp_rx(struct pktbuf *frm, struct ether_hdr *ehdr);
void arp_tx(struct pktbuf *packet, in_addr_t dstaddr, u16 proto);
