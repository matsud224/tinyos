#pragma once
#include <kern/kernlib.h>
#include <net/inet/protohdr.h>

void udp_rx(struct pktbuf *pkt, struct ip_hdr *iphdr);
