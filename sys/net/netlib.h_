#pragma once
#include <kern/kernlib.h>
#include <net/net.h>

#define SOCK_UNUSED    0
#define SOCK_RESERVED  1
#define SOCK_STREAM    2 //TCP
#define SOCK_DGRAM    3 //UDP

#define TIMEOUT_NOTUSE TMO_FEVR

int socket(int type);
int bind(int s, in_port_t my_port);
int close(int s);
int sendto(int s, const char *msg, u32 len, int flags, in_addr_t to_addr, in_port_t to_port);
int recvfrom(int s, char *buf, u32 len, int flags, in_addr_t from_addr, in_port_t *from_port, TMO timeout);
int connect(int s, in_addr_t to_addr[], in_port_t to_port, TMO timeout);
int listen(int s, int backlog);
int accept(int s, in_addr_t client_addr, in_port_t *client_port, TMO timeout);
int send(int s, const char *msg, u32 len, int flags, TMO timeout);
int recv(int s, char *buf, u32 len, int flags, TMO timeout);
int recv_line(int s, char *buf, u32 len, int flags, TMO timeout);

int find_unusedsocket(void);
