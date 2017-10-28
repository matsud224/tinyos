#pragma once
#include "protohdr.h"

void ipv4_rx(struct pktbuf_head *frm, ipv4_hdr *iphdr);
void ipv4_tx(struct pktbuf_head *data, u8 *dstaddr, u8 proto);
