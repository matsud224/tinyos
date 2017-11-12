#include <kern/list.h>

void queue_init(struct queue_head *hdr, int len) {
  hdr->free = len;
  list_init(&hdr->list);
}

