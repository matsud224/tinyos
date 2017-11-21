#include <kern/list.h>
#include <kern/kernlib.h>

void list_init(struct list_head *hdr) {
  hdr->next = hdr;
  hdr->prev = hdr;
}

int list_is_empty(struct list_head *list) {
  return list->next == list;
}

void list_pushfront(struct list_head *item, struct list_head *list) {
  item->next = list->next;
  item->prev = list;
  list->next->prev = item;
  list->next = item;
}

void list_pushback(struct list_head *item, struct list_head *list) {
  item->next = list;
  item->prev = list->prev;
  list->prev->next = item;
  list->prev = item;
}

void list_rotate_forward(struct list_head *list) {
  if(list_is_empty(list))
    return;
  struct list_head *next = list->next;
  list_remove(list);
  list_pushfront(list, next);
}

void list_rotete_backward(struct list_head *list) {
  if(list_is_empty(list))
    return;
  struct list_head *prev = list->prev;
  list_remove(list);
  list_pushback(list, prev);
}

void list_remove(struct list_head *item) {
  item->prev->next = item->next;
  item->next->prev = item->prev;
  list_init(item);
}

struct list_head *list_pop(struct list_head *list) {
  if(list_is_empty(list))
    return NULL;
  struct list_head *first = list->next;
  list_remove(first);
  return first;
}

