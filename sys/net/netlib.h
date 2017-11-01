#pragma once
#include "types.h"
#include "protohdr.h"

#define SOCK_UNUSED    0
#define SOCK_RESERVED  1
#define SOCK_STREAM    2 //TCP
#define SOCK_DGRAM    3 //UDP

#define TIMEOUT_NOTUSE TMO_FEVR

int socket(int type);
int bind(int s, u16 my_port);
int close(int s);
int sendto(int s, const char *msg, u32 len, int flags, u8 to_addr[], u16 to_port);
int recvfrom(int s, char *buf, u32 len, int flags, u8 from_addr[], u16 *from_port, TMO timeout);
int connect(int s, u8 to_addr[], u16 to_port, TMO timeout);
int listen(int s, int backlog);
int accept(int s, u8 client_addr[], u16 *client_port, TMO timeout);
int send(int s, const char *msg, u32 len, int flags, TMO timeout);
int recv(int s, char *buf, u32 len, int flags, TMO timeout);
int recv_line(int s, char *buf, u32 len, int flags, TMO timeout);

int find_unusedsocket(void);
