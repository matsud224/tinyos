#include "pktbuf.h"

struct pktbuf_head *pktbuf_create(uint8_t *buf, uint32_t size, void (*freefunc)(uint8_t *)) {
  struct pktbuf_head *head = malloc(sizeof(struct pktbuf_head));
  head->total = head->size = size;
  head->head = head->data = buf;
  head->end = head_tail = buf + size;
  
  head->next_frag = NULL;
  list_init(&head->pkt_link);

  head->freefunc = freefunc;

  return head;
}


struct pktbuf_head *pktbuf_alloc(uint32_t size) {
  return pktbuf_create(malloc(size), size, free);
}

static void pktbuf_free_one(struct pktbuf_head *head) {
  if(head->freefunc != NULL)
    head->freefunc(head->head);
  free(head);
}

void pktbuf_free(struct pktbuf_head *head) {
  struct pktbuf_head *next = NULL;
  do{
    next = head->next_frag;
    pktbuf_free_one(head);
    head = next;
  }while(head!=NULL);
}

int pktbuf_reserve(struct pktbuf_head *head, uint32_t size) {
  if(head->data + size > head->end) {
    return -1;
  }
  head->data += size;
  head->tail = MAX(head->data, head->tail);
  return 0;
}

uint8_t *pktbuf_add_header(uint32_t size) {
  if(head->head + (size-1) > head->data) {
    return NULL;
  }
  head->data -= size-1;
  head->size += size;
  head->total += size;
  return head->data;
}

void pktbuf_remove_header(uint32_t size) {
  head->data += size;
  head->size -= size;
  head->totalsize -= size;
}

int pktbuf_write_fragment(struct pktbuf_head *head, uint8_t *buf, uint32_t size) {
  if(head->next_frag)
    return -1;
  if(head->tail+size < head->end)
    return -1;
  memcpy(head->tail, buf, size);
  head->tail += size;
  head->size += size;
  head->total += size;
  return 0;
}

void pktbuf_add_fragment(struct pktbuf_head *head, struct pktbuf_head *frag) {
  struct pktbuf_head **last = &head->next_frag;
  while(*last != NULL)
    last = &((*last)->next_frag);
  *last = frag;
  head->total += frag->total;
}

int pktbuf_is_nonlinear(struct pktbuf_head *head) {
  return head->total != head->size;
}


