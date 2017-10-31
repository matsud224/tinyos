#pragma once
#include <kern/kernlib.h>
#include <net/inet/protohdr.h>

void udp_rx(struct pktbuf_head *pkt, struct ip_hdr *iphdr);
ssize_t udp_recvfrom(udp_ctrlblock *ucb, char *buf, uint32_t len, int flags, uint8_t from_addr[], uint16_t *from_port, TMO timeout);
int udp_sendto(const char *msg, uint32_t len, int flags, in_addr_t to_addr, in_port_t to_port, in_port_t my_port);
