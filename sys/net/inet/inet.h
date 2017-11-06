#pragma once
#include <net/net.h>
#include <kern/netdev.h>
#include <kern/kernlib.h>

typedef u32 in_addr_t;
typedef u16 in_port_t;

struct sockaddr_in {
  u8 len;
  u8 family;
  in_port_t port;
  in_addr_t addr;
};

struct ifaddr_in {
  struct list_head dev_link;
  struct list_head family_link;
  struct netdev *dev;
  u8 len;
  u8 family;
  in_addr_t addr;
  in_addr_t netmask;
  u8 flags;
};
