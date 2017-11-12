#pragma once
#include <kern/list.h>

struct queue_head {
  int free;
  struct list_head list;
};

void queue_init(struct queue_head *hdr, int len);
#define queue_enqueue(item, q) list_pushback(item, &(q)->list)
#define queue_dequeue(q) list_pop(&(q)->list)
#define queue_entry list_entry
#define queue_is_full(q) ((q)->free == 0)
#define queue_is_empty(q) list_is_empty(&(q)->list)
