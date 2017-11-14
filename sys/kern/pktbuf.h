#pragma once
#include <kern/kernlib.h>

struct pktbuf {
  struct list_head link;

  u8 *begin;
  u8 *head;
  u8 *tail;
  u8 *end;

  void (*freefunc)(u8 *);
};

#define pktbuf_get_size(pkt) ((size_t)((pkt)->tail - (pkt)->head))

struct pktbuf *pktbuf_create(u8 *buf, size_t size, void (*freefunc)(u8 *));
struct pktbuf *pktbuf_alloc(size_t size);
void pktbuf_free(struct pktbuf *pkt);
int pktbuf_reserve_headroom(struct pktbuf *pkt, size_t size);
u8 *pktbuf_add_header(struct pktbuf *pkt, size_t size);
void pktbuf_remove_header(struct pktbuf *pkt, size_t size);
void pktbuf_copyin(struct pktbuf *pkt, const u8 *data, size_t size, u32 offset);
