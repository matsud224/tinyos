#pragma once

struct list_head {
  struct list_head *next;
  struct list_head *prev;
};

void list_init(struct list_head *hdr);
int list_is_empty(struct list_head *list);
void list_pushfront(struct list_head *item, struct list_head *list);
void list_pushback(struct list_head *item, struct list_head *list);
#define list_insert_front list_pushfront
#define list_insert_back list_pushback
void list_rotate_forward(struct list_head *list);
void list_rotate_backward(struct list_head *list);
void list_remove(struct list_head *item);
struct list_head *list_pop(struct list_head *list);

#define list_foreach(p, lst) for((p)=(lst)->next; (p)!=(lst); (p)=(p)->next)
#define list_foreach_safe(p, tmp, lst) for((p)=(lst)->next, (tmp)=(p)->next; (p)!=(lst); (p)=(tmp), (tmp)=(p)->next)
#define list_entry container_of
