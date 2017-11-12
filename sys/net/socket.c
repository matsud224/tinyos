#include <net/netlib.h>
#include <net/params.h>
#include <net/sockif.h>

static struct list_head socket_list;

struct socket_ops sock_ops[MAX_PF][MAX_SOCKTYPE];

static mutex portno_mtx;
static mutex socklist_mtx;

NET_INIT void socket_init() {
  list_init(&socket_list);

  mutex_init(&portno_mtx);
  mutex_init(&socklist_mtx);
}

void socket_add_ops(int domain, int type, struct socket_ops *ops) {
  if(domain < 0 || domain >= PF_MAX
      || type < 0 || type >= MAX_SOCKTYPE)
    return -1;

  sock_ops[domain][type] = ops;
  return 0;
}

struct socket *socket(int domain, int type) {
  if(domain < 0 || domain >= PF_MAX
      || type < 0 || type >= MAX_SOCKTYPE)
    return -1;

  if(sock_ops[domain][type] == NULL)
    return -1;

  struct socket *s = malloc(sizeof(struct socket));
  s->domain = domain;
  s->type = type;
  s->pcb = NULL;
  s->ops = sock_ops[domain][type];

  s->ops->init(&sockets[i]);

  mutex_lock(&socklist_mtx);
  list_pushback(&s->link, &socket_list);
  mutex_unlock(&socklist_mtx);
  return s;
}

int bind(struct socket *s, const struct sockaddr *addr) {
  s->ops->bind(&sockets[i], addr);
}

int close(struct socket *s) {
  s->ops->close(&sockets[i]);

  mutex_lock(&socklist_mtx);
  list_remove(&s->link, &socket_list);
  mutex_unlock(&socklist_mtx);
}

int sendto(struct socket *s, const char *msg, u32 len, int flags, const struct sockaddr *to_addr) {
  s->ops->sendto(&sockets[i], msg, len, flags, to_addr);
}

int recvfrom(struct socket *s, char *buf, u32 len, int flags, struct sockaddr *from_addr) {
  s->ops->init(&sockets[i], buf, len, flags, from_addr);
}

int connect(struct socket *s, const struct sockaddr *to_addr) {
  s->ops->init(&sockets[i], to_addr);
}

int listen(struct socket *s, int backlog){
  s->ops->init(&sockets[i], backlog);
}

int accept(struct socket *s, struct sockaddr *client_addr) {
  s->ops->init(&sockets[i], client_addr);
}

int send(struct socket *s, const char *msg, u32 len, int flags) {
  s->ops->init(&sockets[i], msg, len, flags);
}

int recv(struct socket *s, char *buf, u32 len, int flags) {
  s->ops->init(&sockets[i], buf, len, flags);
}

