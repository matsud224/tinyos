#pragma once
#include <net/inet/protohdr.h>

void ip_rx(struct pktbuf *frm, ip_hdr *iphdr);
void ip_tx(struct pktbuf *data, u8 *dstaddr, u8 proto);
void ip_set_defaultgw(in_addr_t addr);
in_addr_t ip_get_defaultgw();
