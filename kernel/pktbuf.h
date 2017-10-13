#pragma once

#include "kernlib.h"

struct pktbuf_head;
struct pktbuf_fragment {
  struct pktbuf_fragment *next;
  struct pktbuf_head *parent;

  u32 size;
  u8 *head;

  void (*freefunc)(u8 *);
};

struct pktbuf_head {
  struct pktbuf_fragment *next_frag;
  struct list_head pkt_link;

  u32 total;
  u32 size;
  u8 *head;
  u8 *data;
  u8 *tail;
  u8 *end;

  void (*freefunc)(u8 *);
};

struct pktbuf_head *pktbuf_create(u8 *buf, u32 size, void (*freefunc)(u8 *));
struct pktbuf_head *pktbuf_alloc(u32 size);
void pktbuf_free(struct pktbuf_head *head);
int pktbuf_reserve_header(struct pktbuf_head *head, u32 size);
u8 *pktbuf_add_header(struct pktbuf_head *head, u32 size);
void pktbuf_remove_header(struct pktbuf_head *head, u32 size);
int pktbuf_write_fragment(struct pktbuf_head *head, u8 *buf, u32 size);
void pktbuf_add_fragment(struct pktbuf_head *head, u8 *buf, u32 size, void (*freefunc)(u8 *));
int pktbuf_is_nonlinear(struct pktbuf_head *head);

