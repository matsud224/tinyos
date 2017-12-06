#pragma once
#include <kern/list.h>

struct queue_head {
  int free;
  struct list_head list;
};

void queue_init(struct queue_head *hdr, int len);
int queue_enqueue(struct list_head *item, struct queue_head *q);
#define queue_dequeue(q) list_pop(&(q)->list)
#define queue_is_full(q) ((q)->free == 0)
#define queue_is_empty(q) (list_is_empty(&(q)->list))
#define queue_free_all(q, type, memb, func) list_free_all(&((q)->list), type, memb, func)
