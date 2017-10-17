#pragma once
#include "protohdr.h"

void start_ip(void);
void ip_process(ether_flame *flm, ip_hdr *iphdr); //下位層からのパケットを処理
void ip_send(hdrstack *data, u8 *dstaddr, u8 proto);
u16 ip_getid(void);
