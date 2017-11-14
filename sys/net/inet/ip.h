#pragma once
#include <net/inet/protohdr.h>

#define ip_header_len(iphdr) (iphdr->ip_hl*4)

void ip_rx(struct pktbuf *pkt);
void ip_tx(struct pktbuf *data, in_addr_t srcaddr, in_addr_t dstaddr, u8 proto);
void ip_set_defaultgw(in_addr_t addr);
in_addr_t ip_get_defaultgw();
struct netdev *ip_routing(in_addr_t orig_src, in_addr_t orig_dst, in_addr_t *src, in_addr_t *dst);
