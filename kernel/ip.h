#pragma once

#include "protohdr.h"

void start_ip(void);
void ip_process(ether_flame *flm, ip_hdr *iphdr); //下位層からのパケットを処理
void ip_send(hdrstack *data, uint8_t *dstaddr, uint8_t proto);
uint16_t ip_getid(void);
