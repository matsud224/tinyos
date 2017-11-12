#pragma once
#include <kern/machine.h>
#include <net/socket.h>
#include <kern/netdev.h>
#include <kern/kernlib.h>

typedef u32 in_addr_t;
typedef u16 in_port_t;

#define INADDR_ANY 0

#ifdef ENDIAN_BE
#define IPADDR(a,b,c,d) (((a)<<24)|((b)<<16)|((c)<<8)|(d))
#else
#define IPADDR(a,b,c,d) ((a)|((b)<<8)|((c)<<16)|((d)<<24))
#endif

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
