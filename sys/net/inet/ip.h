#pragma once
#include <net/inet/protohdr.h>

void ip_rx(struct pktbuf_head *frm, ip_hdr *iphdr);
void ip_tx(struct pktbuf_head *data, u8 *dstaddr, u8 proto);
