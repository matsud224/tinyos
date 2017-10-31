#pragma once
#include <net/inet/protohdr.h>

void icmp_rx(struct pktbuf_head *frm, ip_hdr *iphdr);
