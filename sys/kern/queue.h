#pragma once
#include <kern/list.h>

struct queue_head {
  size_t free;
  struct list_head list;
};

void queue_init(struct queue_head *hdr, size_t len);
int queue_enqueue(struct list_head *item, struct queue_head *q);
struct list_head *queue_dequeue(struct queue_head *q);
#define queue_is_full(q) ((q)->free == 0)
#define queue_is_empty(q) list_is_empty(&(q)->list)
#define queue_free_all(q, type, memb, func) do { \
    struct list_head *_p, *_tmp; \
    list_foreach_safe(_p, _tmp, &((q)->list)) { \
      list_remove(_p); \
      (func)(list_entry(_p, type, memb)); \
      (q)->free++; \
    } \
  } while(0)
