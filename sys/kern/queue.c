#include <kern/list.h>
#include <kern/queue.h>

void queue_init(struct queue_head *hdr, int len) {
  hdr->free = len;
  list_init(&hdr->list);
}

int queue_enqueue(struct list_head *item, struct queue_head *q) {
  if(queue_is_full(q))
    return -1;
  list_pushback(item, &q->list);
  return 0;
}

