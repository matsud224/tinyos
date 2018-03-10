#pragma once
#include <kern/kernlib.h>

struct pktbuf {
  struct list_head link;
  char *begin;
  char *head;
  char *tail;
  char *end;
  void (*freefunc)(void *);
};

#define pktbuf_get_size(pkt) ((size_t)((pkt)->tail - (pkt)->head))

struct pktbuf *pktbuf_create(char *buf, size_t size, void (*freefunc)(void *));
struct pktbuf *pktbuf_alloc(size_t size);
void pktbuf_free(struct pktbuf *pkt);
int pktbuf_reserve_headroom(struct pktbuf *pkt, size_t size);
char *pktbuf_add_header(struct pktbuf *pkt, size_t size);
void pktbuf_remove_header(struct pktbuf *pkt, size_t size);
void pktbuf_copyin(struct pktbuf *pkt, const char *data, size_t size, off_t offset);
