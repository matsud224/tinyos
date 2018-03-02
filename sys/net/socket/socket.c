#include <net/socket/params.h>
#include <net/socket/socket.h>
#include <kern/lock.h>
#include <kern/kernlib.h>
#include <kern/file.h>

static struct list_head socket_list;

struct socket_ops *sock_ops[MAX_PF][MAX_SOCKTYPE];

static mutex portno_mtx;
static mutex socklist_mtx;

int sock_read(struct file *f, void *buf, size_t count);
int sock_write(struct file *f, const void *buf, size_t count);
int sock_lseek(struct file *f, off_t offset, int whence);
int sock_close(struct file *f);
int sock_sync(struct file *f);

static const struct file_ops sock_file_ops = {
  .read = sock_read,
  .write = sock_write,
  .lseek = sock_lseek, 
  .close = sock_close,
  .sync = sock_sync,
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
  return file_new(FILE_SOCKET, s, sock_file_ops);
}

int bind(struct file *f, const struct sockaddr *addr) {
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  return s->ops->bind(s->pcb, addr);
}

int sendto(struct file *f, const char *msg, u32 len, int flags, const struct sockaddr *to_addr) {
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  return s->ops->sendto(s->pcb, msg, len, flags, to_addr);
}

int recvfrom(struct file *f, char *buf, u32 len, int flags, struct sockaddr *from_addr) {
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  return s->ops->recvfrom(s->pcb, buf, len, flags, from_addr);
}

int connect(struct file *f, const struct sockaddr *to_addr) {
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  return s->ops->connect(s->pcb, to_addr);
}

int listen(struct file *f, int backlog){
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  return s->ops->listen(s->pcb, backlog);
}

struct socket *accept(struct file *f, struct sockaddr *client_addr) {
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  struct socket *s2 = _socket(s->domain, s->type);
  s2->pcb = s->ops->accept(s->pcb, client_addr);
  return s2;
}

int send(struct file *f, const char *msg, u32 len, int flags) {
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  return s->ops->send(s->pcb, msg, len, flags);
}

int recv(struct file *f, char *buf, u32 len, int flags) {
  if(f->type != FILE_SOCKET)
    return -1;
  struct socket *s = (struct socket *)f->fcb;
  return s->ops->recv(s->pcb, buf, len, flags);
}


int sock_read(struct socket *s, void *buf, size_t count) {
  return recv(s, buf, count, 0);
}

int sock_write(struct socket *s, const void *buf, size_t count) {
  return send(s, buf, count, 0);
}

int sock_lseek(struct socket *s, off_t offset, int whence) {
  return EBADF;
}

int sock_close(struct socket *s) {
  int retval = s->ops->close(s->pcb);

  mutex_lock(&socklist_mtx);
  list_remove(&s->link);
  mutex_unlock(&socklist_mtx);

  return retval;
}

int sock_sync(struct socket *s) {
  return 0;
}


