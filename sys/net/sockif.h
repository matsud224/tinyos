#pragma once
#include <kern/types.h>
#include <net/params.h>

struct sockaddr {
  u8 len;
  u8 family;
  u8 addr[];
}

struct socket_t{
  int type;
  struct sockaddr addr;
};

extern socket_t sockets[MAX_SOCKET];
