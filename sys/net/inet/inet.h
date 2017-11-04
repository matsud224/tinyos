#pragma once
#include <net/net.h>

typedef u32 in_addr_t;
typedef u16 in_port_t;

struct sockaddr_in {
  u8 len;
  u8 family;
  in_port_t port;
  in_addr_t addr;
};

struct ifaddr_in {
  u8 len;
  u8 family;
  in_addr_t addr;
  in_addr_t netmask;
  u8 flags;
};
