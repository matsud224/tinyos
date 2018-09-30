#include <kern/pktbuf.h>

struct pktbuf *pktbuf_create(char *buf, size_t size, void (*freefunc)(void *), int flags) {
  struct pktbuf *pkt = malloc(sizeof(struct pktbuf));
  pkt->begin = pkt->head = buf;
  pkt->end = pkt->tail = buf + size;
  pkt->freefunc = freefunc;
  pkt->is_enabled = 1;
  pkt->flags = flags;
  return pkt;
}

struct pktbuf *pktbuf_alloc(size_t size, int flags) {
  struct pktbuf *pkt = pktbuf_create(malloc(size), size, free, flags);
  pkt->tail = pkt->head;
  pkt->is_enabled = 1;
  return pkt;
}

void pktbuf_free(struct pktbuf *pkt) {
  if(!pkt->is_enabled) {
    puts("------ use of freed pktbuf! ------");
    while(1);
  }
  if(pkt->freefunc != NULL)
    pkt->freefunc(pkt->begin);
  pkt->is_enabled = 0;
  free(pkt);
}

int pktbuf_reserve_headroom(struct pktbuf *pkt, size_t size) {
  if(!pkt->is_enabled) {
    puts("------ use of freed pktbuf! ------");
    while(1);
  }
  if(pkt->head + size > pkt->end) {
    puts("!!!!!!!!reserve_headroom failed");
    while(1);
    return -1;
  }
  pkt->head += size;
  pkt->tail = MAX(pkt->head, pkt->tail);
  return 0;
}

char *pktbuf_add_header(struct pktbuf *pkt, size_t size) {
  if(!pkt->is_enabled) {
    puts("------ use of freed pktbuf! ------");
    while(1);
  }
  if(pkt->begin + size > pkt->head) {
    size_t diff = (u32)pkt->begin + size - (u32)pkt->head;
    printf("!!!!!add_header failed! %x total=%d, size=%d, request:%d\n", pkt, (u32)pkt->end-(u32)pkt->begin, (u32)pkt->tail-(u32)pkt->head, size);
    while(1);
    return NULL;
  }
  pkt->head -= size;
  return pkt->head;
}

void pktbuf_remove_header(struct pktbuf *pkt, size_t size) {
  if(!pkt->is_enabled) {
    puts("------ use of freed pktbuf! ------");
    while(1);
  }
  pkt->head += size;
}

void pktbuf_copyin(struct pktbuf *pkt, const char *data, size_t size, off_t offset) {
  if(!pkt->is_enabled) {
    puts("------ use of freed pktbuf! ------");
    while(1);
  }
  if(pkt->head + offset + size > pkt->tail)
    pkt->tail = pkt->head + offset + size;
  memcpy(pkt->head+offset, data, size);
}

