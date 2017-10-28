#pragma once
#include "types.h"
#include "protohdr.h"
#include "netconf.h"

struct udp_ctrlblock;
struct tcp_ctrlblock;

struct transport_addr{
    u16 my_port;
    u16 partner_port;
    u8 partner_addr[IP_ADDR_LEN];
};

struct socket_t{
    int type;

    union{
		udp_ctrlblock *ucb;
		tcp_ctrlblock *tcb;
    } ctrlblock;

    //以下の２つはctrlblockにコピーされる
    ID ownertsk;
    transport_addr addr;
};

extern socket_t sockets[MAX_SOCKET];
