#pragma once

#include <kernel.h>

#include <stdint.h>

#include "protohdr.h"

struct udp_ctrlblock;

void udp_process(ether_flame *flm, ip_hdr *iphdr, udp_hdr *uhdr);

udp_ctrlblock *ucb_new(ID owner);
void udp_disposecb(udp_ctrlblock *ucb);

int32_t udp_recvfrom(udp_ctrlblock *ucb, char *buf, uint32_t len, int flags, uint8_t from_addr[], uint16_t *from_port, TMO timeout);
int udp_sendto(const char *msg, uint32_t len, int flags, uint8_t to_addr[], uint16_t to_port, uint16_t my_port);
