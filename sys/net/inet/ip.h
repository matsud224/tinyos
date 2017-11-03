#pragma once
#include <net/inet/protohdr.h>

void ip_rx(struct pktbuf *frm, ip_hdr *iphdr);
void ip_tx(struct pktbuf *data, u8 *dstaddr, u8 proto);
