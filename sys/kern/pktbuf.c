#include <kern/pktbuf.h>

struct pktbuf *pktbuf_create(char *buf, size_t size, void (*freefunc)(void *)) {
  struct pktbuf *pkt = malloc(sizeof(struct pktbuf));
  pkt->begin = pkt->head = buf;
  pkt->end = pkt->tail = buf + size;
  pkt->freefunc = freefunc;
  return pkt;
}

struct pktbuf *pktbuf_alloc(size_t size) {
  struct pktbuf *pkt = pktbuf_create(malloc(size), size, free);
  pkt->tail = pkt->head;
  return pkt;
}

void pktbuf_free(struct pktbuf *pkt) {
  if(pkt->freefunc != NULL)
    pkt->freefunc(pkt->begin);
  free(pkt);
}

int pktbuf_reserve_headroom(struct pktbuf *pkt, size_t size) {
  if(pkt->head + size > pkt->end) {
    return -1;
  }
  pkt->head += size;
  pkt->tail = MAX(pkt->head, pkt->tail);
  return 0;
}

char *pktbuf_add_header(struct pktbuf *pkt, size_t size) {
  if(pkt->begin + size > pkt->head) {
    return NULL;
  }
  pkt->head -= size;
  return pkt->head;
}

void pktbuf_remove_header(struct pktbuf *pkt, size_t size) {
  pkt->head += size;
}

void pktbuf_copyin(struct pktbuf *pkt, const char *data, size_t size, off_t offset) {
  if(pkt->head + offset + size > pkt->tail)
    pkt->tail = pkt->head + offset + size;
  memcpy(pkt->head+offset, data, size);
}

