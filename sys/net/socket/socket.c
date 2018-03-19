#include <net/socket/params.h>
#include <net/socket/socket.h>
#include <kern/lock.h>
#include <kern/kernlib.h>
#include <kern/file.h>

static struct list_head socket_list;

const struct socket_ops *sock_ops[MAX_PF][MAX_SOCKTYPE];

static mutex portno_mtx;
static mutex socklist_mtx;

int sock_read(struct file *f, void *buf, size_t count);
int sock_write(struct file *f, const void *buf, size_t count);
int sock_close(struct file *f);

static const struct file_ops sock_file_ops = {
  .read = sock_read,
  .write = sock_write,
  .close = sock_close,
};


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

static struct socket *_socket(int domain, int type) {
  if(domain < 0 || domain >= MAX_PF
      || type < 0 || type >= MAX_SOCKTYPE)
    return NULL;

  if(sock_ops[domain][type] == NULL)
    return NULL;

  struct socket *s = malloc(sizeof(struct socket));
  s->domain = domain;
  s->type = type;
  s->ops = sock_ops[domain][type];
  s->pcb = s->ops->init();
  mutex_lock(&socklist_mtx);
  list_pushback(&s->link, &socket_list);
  mutex_unlock(&socklist_mtx);
  return s;
}

struct file *socket(int domain, int type) {
  struct socket *s = _socket(domain, type);
  if(s == NULL)
    return NULL;
  s->pcb = s->ops->init();
  return file_new(s, &sock_file_ops, _FREAD | _FWRITE);
}

static int is_sock_file(struct file *f) {
  return f->ops == &sock_file_ops;
}

int bind(struct file *f, const struct sockaddr *addr) {
  if(!is_sock_file(f))
    return -1;
  struct socket *s= (struct socket *)f->data;
  if(s->ops->bind)
    return s->ops->bind(s->pcb, addr);
  else
    return -1;
}

int sendto(struct file *f, const char *msg, size_t len, int flags, const struct sockaddr *to_addr) {
  if(!is_sock_file(f))
    return -1;
  struct socket *s = (struct socket *)f->data;
  if(s->ops->sendto)
    return s->ops->sendto(s->pcb, msg, len, flags, to_addr);
  else
    return -1;
}

int recvfrom(struct file *f, char *buf, size_t len, int flags, struct sockaddr *from_addr) {
  if(!is_sock_file(f))
    return -1;
  struct socket *s = (struct socket *)f->data;
  if(s->ops->recvfrom)
    return s->ops->recvfrom(s->pcb, buf, len, flags, from_addr);
  else
    return -1;
}

int connect(struct file *f, const struct sockaddr *to_addr) {
  if(!is_sock_file(f))
    return -1;
  struct socket *s = (struct socket *)f->data;
  if(s->ops->connect)
    return s->ops->connect(s->pcb, to_addr);
  else
    return -1;
}

int listen(struct file *f, int backlog){
  if(!is_sock_file(f))
    return -1;
  struct socket *s = (struct socket *)f->data;
  if(s->ops->listen)
    return s->ops->listen(s->pcb, backlog);
  else
    return -1;
}

struct file *accept(struct file *f, struct sockaddr *client_addr) {
  if(!is_sock_file(f))
    return NULL;
  struct socket *s = (struct socket *)f->data;
  if(!s->ops->accept)
    return NULL;

  struct socket *s2 = _socket(s->domain, s->type);
  s2->pcb = s->ops->accept(s->pcb, client_addr);
  return file_new(s2, &sock_file_ops, _FREAD | _FWRITE);
}

int send(struct file *f, const char *msg, size_t len, int flags) {
  if(!is_sock_file(f))
    return -1;
  struct socket *s = (struct socket *)f->data;
  if(s->ops->send)
    return s->ops->send(s->pcb, msg, len, flags);
  else
    return -1;
}

int recv(struct file *f, char *buf, size_t len, int flags) {
  if(!is_sock_file(f))
    return -1;
  struct socket *s = (struct socket *)f->data;
  if(s->ops->recv)
    return s->ops->recv(s->pcb, buf, len, flags);
  else
    return -1;
}


int sock_read(struct file *f, void *buf, size_t count) {
  return recv(f, buf, count, 0);
}

int sock_write(struct file *f, const void *buf, size_t count) {
  return send(f, buf, count, 0);
}

int sock_close(struct file *f) {
  struct socket *s = (struct socket *)f->data;
  int retval = 0;
  if(s->ops->close)
    retval = s->ops->close(s->pcb);

  mutex_lock(&socklist_mtx);
  list_remove(&s->link);
  mutex_unlock(&socklist_mtx);

  return retval;
}



