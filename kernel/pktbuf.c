#include "pktbuf.h"

struct pktbuf_head *pktbuf_create(u8 *buf, u32 size, void (*freefunc)(u8 *)) {
  struct pktbuf_head *head = malloc(sizeof(struct pktbuf_head));
  head->total = head->size = 0;
  head->head = head->data = head->tail = buf;
  head->end = buf + size;
  
  head->next_frag = NULL;
  list_init(&head->pkt_link);

  head->freefunc = freefunc;

  return head;
}

struct pktbuf_head *pktbuf_alloc(u32 size) {
  return pktbuf_create(malloc(size), size, free);
}

static void pktbuf_free_fragment(struct pktbuf_fragment *frag) {
  if(frag->freefunc != NULL)
    frag->freefunc(frag->head);
  free(frag);
}

void pktbuf_free(struct pktbuf_head *head) {
  struct pktbuf_fragment *frag = head->next_frag;
  struct pktbuf_fragment *next = NULL;
  while(frag != NULL) {
    next = head->next_frag;
    pktbuf_free_fragment(frag);
    frag = next;
  }

  if(head->freefunc != NULL)
    head->freefunc(head->head);
  free(head);
}

int pktbuf_reserve_header(struct pktbuf_head *head, u32 size) {
  if(head->size > 0 || head->data + size > head->end) {
    return -1;
  }
  head->data += size;
  head->tail = MAX(head->data, head->tail);
  return 0;
}

u8 *pktbuf_add_header(struct pktbuf_head *head, u32 size) {
  if(head->head + size > head->data) {
    return NULL;
  }
  head->data -= size;
  head->size += size;
  head->total += size;
  return head->data;
}

void pktbuf_remove_header(struct pktbuf_head *head, u32 size) {
  head->data += size;
  head->size -= size;
  head->total -= size;
}

int pktbuf_write_fragment(struct pktbuf_head *head, u8 *buf, u32 size) {
  if(head->next_frag)
    return -1;
  if(head->tail+size > head->end)
    return -1;
  memcpy(head->tail, buf, size);
  head->tail += size;
  head->size += size;
  head->total += size;
  return 0;
}

void pktbuf_add_fragment(struct pktbuf_head *head, u8 *buf UNUSED, u32 size, void (*freefunc)(u8 *)) {
  struct pktbuf_fragment *frag = malloc(sizeof(struct pktbuf_fragment));
  frag->next = NULL;
  frag->parent = head;
  frag->size = size;
  frag->head = head;
  frag->freefunc = freefunc;

  struct pktbuf_fragment **last = &head->next_frag;
  while(*last != NULL)
    last = &((*last)->next);
  *last = frag;
  head->total += frag->size;
}

int pktbuf_is_nonlinear(struct pktbuf_head *head) {
  return head->total != head->size;
}


