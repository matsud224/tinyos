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
  const struct socket_ops *ops;
  void *pcb;
};

struct socket_ops {
  void *(*init)(void);
  int (*bind)(void *pcb, const struct sockaddr *addr);
  int (*close)(void *pcb);
  int (*connect)(void *pcb, const struct sockaddr *addr);
  int (*listen)(void *pcb, int backlog);
  void *(*accept)(void *pcb, struct sockaddr *client_addr);
  int (*sendto)(void *pcb, const char *msg, size_t len, int flags, const struct sockaddr *dest_addr);
  int (*recvfrom)(void *pcb, char *buf, size_t len, int flags, struct sockaddr *from_addr);
  int (*send)(void *pcb, const char *msg, size_t len, int flags);
  int (*recv)(void *pcb, char *buf, size_t len, int flags);
};

int socket_register_ops(int domain, int type, const struct socket_ops *ops);

struct file *socket(int domain, int type);
int bind(struct file *f, const struct sockaddr *addr);
int sendto(struct file *f, const char *msg, size_t len, int flags, const struct sockaddr *to_addr);
int recvfrom(struct file *f, char *buf, size_t len, int flags, struct sockaddr *from_addr);
int connect(struct file *f, const struct sockaddr *to_addr);
int listen(struct file *f, int backlog);
struct file *accept(struct file *f, struct sockaddr *client_addr);
int send(struct file *f, const char *msg, size_t len, int flags);
int recv(struct file *f, char *buf, size_t len, int flags);

int sys_socket(int domain, int type);
int sys_bind(int fd, const struct sockaddr *addr);
int sys_sendto(int fd, const char *msg, size_t len, int flags, const struct sockaddr *to_addr);
int sys_recvfrom(int fd, char *buf, size_t len, int flags, struct sockaddr *from_addr);
int sys_connect(int fd, const struct sockaddr *to_addr);
int sys_listen(int fd, int backlog);
int sys_accept(int fd, struct sockaddr *client_addr);
int sys_send(int fd, const char *msg, size_t len, int flags);
int sys_recv(int fd, char *buf, size_t len, int flags);
 
