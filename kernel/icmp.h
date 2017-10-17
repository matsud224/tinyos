#pragma once

#include "protohdr.h"

void icmp_process(ether_flame *flm, ip_hdr *iphdr, icmp *icmpdata);
