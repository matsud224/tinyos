#pragma once
#include <kern/kernlib.h>

#define PF_LINK			0
#define PF_INET			1
#define MAX_PF			2

#define SOCK_STREAM			0
#define SOCK_DGRAM			1
#define MAX_SOCKTYPE		2

struct sockaddr {
  u8 len;
  u8 family;
  u8 addr[];
};

struct socket_ops;

struct socket {
  struct list_head link;
  int domain;
  int type;
  struct socket_ops *ops;
  void *pcb;
};

struct socket_ops {
  void *(*init)(void);
  int (*bind)(void *pcb, const struct sockaddr *addr);
  int (*close)(void *pcb);
  int (*connect)(void *pcb, const struct sockaddr *addr);
  int (*listen)(void *pcb, int backlog);
  void *(*accept)(void *pcb, struct sockaddr *client_addr);
  int (*sendto)(void *pcb, const u8 *msg, size_t len, int flags, struct sockaddr *dest_addr);
  int (*recvfrom)(void *pcb, u8 *buf, size_t len, int flags, struct sockaddr *from_addr);
  int (*send)(void *pcb, const u8 *msg, size_t len, int flags);
  int (*recv)(void *pcb, u8 *buf, size_t len, int flags);
};

int socket_register_ops(int domain, int type, const struct socket_ops *ops);

struct socket *socket(int domain, int type);
int bind(struct socket *s, const struct sockaddr *addr);
int close(struct socket *s);
int sendto(struct socket *s, const char *msg, u32 len, int flags, const struct sockaddr *to_addr);
int recvfrom(struct socket *s, char *buf, u32 len, int flags, struct sockaddr *from_addr);
int connect(struct socket *s, const struct sockaddr *to_addr);
int listen(struct socket *s, int backlog);
struct socket *accept(struct socket *s, struct sockaddr *client_addr);
int send(struct socket *s, const char *msg, u32 len, int flags);
int recv(struct socket *s, char *buf, u32 len, int flags);

