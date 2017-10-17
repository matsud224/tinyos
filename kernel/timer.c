#include "timer.h"
#include "kernlib.h"

struct timer {
  struct timer *next;
  u32 expire;
  void (*func)(void *);
  void *arg;
};

static struct timer *timer_head = NULL;

void timer_start(u32 expire, void (*func)(void *), void *arg) {
  struct timer *t = malloc(sizeof(struct timer));
  t->expire = expire;
  t->func = func;
  t->arg = arg;
  struct timer **p = &timer_head;
  while(*p!=NULL) {
    if(t->expire < (*p)->expire) {
      (*p)->expire -= t->expire;
      break;
    }
    t->expire -= (*p)->expire;
    *p = &((*p)->next);
  }

  t->next = *p;
  *p = t;
}


void timer_tick() {
  if(timer_head == NULL)
    return;
  else
    timer_head->expire--;

  while(timer_head != NULL && timer_head->expire == 0) {
    struct timer *tmp = timer_head;
    timer_head = timer_head->next;
    (tmp->func)(tmp->arg);
    free(tmp);
  }
}
