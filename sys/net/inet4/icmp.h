#pragma once

#include "protohdr.h"

void icmp_rx(struct pktbuf_head *frm, ip_hdr *iphdr, icmp *icmpdata);
