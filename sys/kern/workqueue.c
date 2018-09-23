#include <kern/workqueue.h>
#include <kern/thread.h>
#include <kern/timer.h>
#include <kern/kernlib.h>

struct workqueue {
  struct list_head queue;
  struct thread *thread;
};

struct work {
  struct list_head link;
  void (*func)(void *);
  void *arg;
  struct workqueue *wq;
};

static void workqueue_thread(void *arg) {
  struct workqueue *wq = (struct workqueue *)arg;
  while(1) {
    struct list_head *item;
    while((cli(), (item = list_pop(&wq->queue))) == NULL) {
      sti();
      thread_sleep(wq);
    }
    sti();
    struct work *w = container_of(item, struct work, link);
    (w->func)(w->arg);
    free(w);
  }
}

struct workqueue *workqueue_new(const char *name) {
  struct workqueue *wq = malloc(sizeof(struct workqueue));
  list_init(&wq->queue);
  wq->thread = kthread_new(workqueue_thread, wq, name);
  thread_run(wq->thread);
  return wq;
}

static void _workqueue_add(const void *arg) {
  struct work *w = (struct work *)arg;
IRQ_DISABLE
  list_pushfront(&w->link, &w->wq->queue);
IRQ_RESTORE
  thread_wakeup(w->wq);
}

void workqueue_add_delayed(struct workqueue *wq, void (*func)(void *), void *arg, int ticks) {
  struct work *w = malloc(sizeof(struct work));
  w->func = func;
  w->arg = arg;
  w->wq = wq;

  if(ticks == 0)
    _workqueue_add(w);
  else
    timer_start(ticks, _workqueue_add, w);
}

void workqueue_add(struct workqueue *wq, void (*func)(void *), void *arg) {
  return workqueue_add_delayed(wq, func, arg, 0);
}


