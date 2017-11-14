#include <net/params.h>
#include <net/socket/socket.h>
#include <kern/lock.h>
#include <kern/kernlib.h>

static struct list_head socket_list;

struct socket_ops *sock_ops[MAX_PF][MAX_SOCKTYPE];

static mutex portno_mtx;
static mutex socklist_mtx;

NET_INIT void socket_init() {
  list_init(&socket_list);

  mutex_init(&portno_mtx);
  mutex_init(&socklist_mtx);
}

int socket_register_ops(int domain, int type, const struct socket_ops *ops) {
  if(domain < 0 || domain >= MAX_PF
      || type < 0 || type >= MAX_SOCKTYPE)
    return -1;

  sock_ops[domain][type] = ops;
  return 0;
}

struct socket *socket(int domain, int type) {
  if(domain < 0 || domain >= MAX_PF
      || type < 0 || type >= MAX_SOCKTYPE)
    return NULL;

  if(sock_ops[domain][type] == NULL)
    return NULL;

  struct socket *s = malloc(sizeof(struct socket));
  s->domain = domain;
  s->type = type;
  s->pcb = s->ops->init();
  s->ops = sock_ops[domain][type];

  mutex_lock(&socklist_mtx);
  list_pushback(&s->link, &socket_list);
  mutex_unlock(&socklist_mtx);
  return s;
}

int bind(struct socket *s, const struct sockaddr *addr) {
  return s->ops->bind(s, addr);
}

int close(struct socket *s) {
  int retval = s->ops->close(s);

  mutex_lock(&socklist_mtx);
  list_remove(&s->link);
  mutex_unlock(&socklist_mtx);

  return retval;
}

int sendto(struct socket *s, const char *msg, u32 len, int flags, const struct sockaddr *to_addr) {
  return s->ops->sendto(s, msg, len, flags, to_addr);
}

int recvfrom(struct socket *s, char *buf, u32 len, int flags, struct sockaddr *from_addr) {
  return s->ops->recvfrom(s, buf, len, flags, from_addr);
}

int connect(struct socket *s, const struct sockaddr *to_addr) {
  return s->ops->connect(s, to_addr);
}

int listen(struct socket *s, int backlog){
  return s->ops->listen(s, backlog);
}

int accept(struct socket *s, struct sockaddr *client_addr) {
  return s->ops->accept(s, client_addr);
}

int send(struct socket *s, const char *msg, u32 len, int flags) {
  return s->ops->send(s, msg, len, flags);
}

int recv(struct socket *s, char *buf, u32 len, int flags) {
  return s->ops->recv(s, buf, len, flags);
}

