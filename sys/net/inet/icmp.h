#pragma once
#include <net/inet/protohdr.h>

void icmp_rx(struct pktbuf *frm, ip_hdr *iphdr);
