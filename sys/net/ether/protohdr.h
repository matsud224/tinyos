#pragma once
#include <kern/types.h>
#include <kern/machine.h>
#include <net/ether/ether.h>

struct ether_hdr{
  struct etheraddr ether_dhost;
  struct etheraddr ether_shost;
  u16 ether_type;
};
#define ETHERTYPE_IP  0x0800
#define ETHERTYPE_ARP  0x0806

