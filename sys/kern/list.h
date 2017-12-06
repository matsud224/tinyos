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
void list_append_front(struct list_head *dst, struct list_head *src);
void list_append_back(struct list_head *dst, struct list_head *src);
void list_remove(struct list_head *item);
struct list_head *list_pop(struct list_head *list);

#define list_foreach(p, lst) for((p)=(lst)->next; (p)!=(lst); (p)=(p)->next)
#define list_foreach_safe(p, tmp, lst) for((p)=(lst)->next, (tmp)=(p)->next; (p)!=(lst); (p)=(tmp), (tmp)=(p)->next)
#define list_foreach_reverse(p, lst) for((p)=(lst)->prev; (p)!=(lst); (p)=(p)->prev)
#define list_foreach_safe_reverse(p, tmp, lst) for((p)=(lst)->prev, (tmp)=(p)->prev; (p)!=(lst); (p)=(tmp), (tmp)=(p)->prev)
#define list_entry container_of
#define list_first(lst) ((lst)->next!=(lst)?(lst)->next:NULL)
#define list_last(lst) ((lst)->prev!=(lst)?(lst)->prev:NULL)

#define list_free_all(list, type, memb, func) do { \
    struct list_head *_p, *_tmp; \
    list_foreach_safe(_p, _tmp, (list)) { \
      list_remove(_p); \
      (func)(list_entry(_p, type, memb)); \
    } \
  } while(0)

