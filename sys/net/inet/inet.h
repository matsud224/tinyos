#pragma once
#include <net/net.h>

typedef in_addr_t u32;
typedef in_port_t u16;

struct sockaddr_in {
  u8 len;
  u8 family;
  in_port_t port;
  in_addr_t addr;
};
