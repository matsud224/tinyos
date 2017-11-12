#pragma once

#include <kernel.h>

#include <stdint.h>

#include "protohdr.h"
#include "sockif.h"

struct tcp_ctrlblock;

void start_tcp(void);
tcp_ctrlblock *tcb_new(void);
void tcb_setaddr_and_owner(tcp_ctrlblock *tcb, transport_addr *addr, ID owner);
transport_addr *tcb_getaddr(tcp_ctrlblock *tcb);
ID tcb_getowner(tcp_ctrlblock *tcb);
void tcb_dispose(tcp_ctrlblock *tcb);
void tcp_process(ether_flame *flm, ip_hdr *iphdr, tcp_hdr *thdr);
uint16_t tcp_get_unusedport(void);
int tcp_connect(tcp_ctrlblock *tcb, TMO timeout);
int tcp_listen(tcp_ctrlblock *tcb, int backlog);
tcp_ctrlblock *tcp_accept(tcp_ctrlblock *tcb, uint8_t client_addr[], uint16_t *client_port, TMO timeout);
int tcp_send(tcp_ctrlblock *tcb, const char *msg, uint32_t len, TMO timeout);
int tcp_receive(tcp_ctrlblock *tcb, char *buf, uint32_t len, TMO timeout);
int tcp_close(tcp_ctrlblock *tcb);
