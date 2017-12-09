#include <kern/timer.h>
#include <kern/kernlib.h>
#include <kern/lock.h>

struct timer_entry {
  struct timer_entry *next;
  u32 expire; 
  void (*func)(void *);
  void *arg;
};

static struct timer_entry *timer_head = NULL;
static mutex timer_mtx;

void timer_start(u32 expire, void (*func)(void *), void *arg) {
  struct timer_entry *t = malloc(sizeof(struct timer_entry));
  t->expire = expire;
  t->func = func;
  t->arg = arg;
  t->next = NULL;
IRQ_DISABLE
  struct timer_entry **p = &timer_head;
  while(*p!=NULL) {
    if(t->expire < (*p)->expire) {
      (*p)->expire -= t->expire;
      break;
    }
    t->expire -= (*p)->expire;
    p = &((*p)->next);
  }
  t->next = *p;
  *p = t;
IRQ_RESTORE
}


void timer_tick() {
  if(timer_head == NULL)
    return;
  else
    if(timer_head->expire > 0)
      timer_head->expire--;

  while(timer_head != NULL && timer_head->expire == 0) {
    struct timer_entry *tmp = timer_head;
    timer_head = timer_head->next;
    (tmp->func)(tmp->arg);
    free(tmp);
  }
}

