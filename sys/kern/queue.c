#include <kern/list.h>
#include <kern/queue.h>

void queue_init(struct queue_head *hdr, int len) {
  hdr->free = len;
  list_init(&hdr->list);
}

