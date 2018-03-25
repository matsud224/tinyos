#include <net/socket/params.h>
#include <net/socket/socket.h>
#include <kern/lock.h>
#include <kern/kernlib.h>
#include <kern/file.h>
#include <kern/thread.h>
#include <kern/syscalls.h>

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
  return file_new(s, &sock_file_ops, FILE_SOCKET, _FREAD | _FWRITE);
}

static int is_sock_file(struct file *f) {
  return f->type == FILE_SOCKET;
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
  return file_new(s2, &sock_file_ops, FILE_SOCKET, _FREAD | _FWRITE);
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

int sys_socket(int domain, int type) {
  int fd = fd_get();
  if(fd < 0)
    return -1;
  current->files[fd] = socket(domain, type);
  if(current->files[fd] == NULL)
    return -1;
  return fd;
}

int sys_bind(int fd, const struct sockaddr *addr) {
  if(fd_check(fd) || buffer_check(addr, sizeof(struct sockaddr)))
    return -1;
  return bind(current->files[fd], addr);
}

int sys_sendto(int fd, const char *msg, size_t len, int flags, const struct sockaddr *to_addr) {
  if(fd_check(fd) || buffer_check(msg, len) || buffer_check(to_addr, sizeof(struct sockaddr)))
    return -1;
  return sendto(current->files[fd], msg, len, flags, to_addr);
}

int sys_recvfrom(int fd, char *buf, size_t len, int flags, struct sockaddr *from_addr) {
  if(fd_check(fd) || buffer_check(buf, len) || buffer_check(from_addr, sizeof(struct sockaddr)))
    return -1;
  return recvfrom(current->files[fd], buf, len, flags, from_addr);
}

int sys_connect(int fd, const struct sockaddr *to_addr) {
  if(fd_check(fd) || buffer_check(to_addr, sizeof(struct sockaddr)))
    return -1;
  return connect(current->files[fd], to_addr);
}

int sys_listen(int fd, int backlog){
  if(fd_check(fd))
    return -1;
  return listen(current->files[fd], backlog);
}

int sys_accept(int fd, struct sockaddr *client_addr) {
  if(fd_check(fd) || buffer_check(client_addr, sizeof(struct sockaddr)))
    return -1;
  int newfd = fd_get();
  if(newfd < 0)
    return -1;
  current->files[newfd] = accept(current->files[fd], client_addr);
  if(current->files[newfd] == NULL)
    return -1;
  return newfd;
}

int sys_send(int fd, const char *msg, size_t len, int flags) {
  if(fd_check(fd) || buffer_check(msg, len))
    return -1;
  return send(current->files[fd], msg, len, flags);
}

int sys_recv(int fd, char *buf, size_t len, int flags) {
  if(fd_check(fd) || buffer_check(buf, len))
    return -1;
  return recv(current->files[fd], buf, len, flags);
}

