#include <kern/workqueue.h>
#include <kern/thread.h>
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

struct workqueue *workqueue_new(char *name) {
  struct workqueue *wq = malloc(sizeof(struct workqueue));
  list_init(&wq->queue);
  wq->thread = kthread_new(workqueue_thread, wq, "wq");
  thread_run(wq->thread);
  return wq;
}

static void _workqueue_add(void *arg) {
  struct work *w = (struct work *)arg;
  list_pushback(&w->link, &w->wq->queue);

  thread_wakeup(w->wq);
}

void workqueue_add_delayed(struct workqueue *wq, void (*func)(void *), void *arg, int delay) {
  struct work *w = malloc(sizeof(struct work));
  w->func = func;
  w->arg = arg;
  w->wq = wq;

  timer_start(delay, _workqueue_add, w);

  return w;
}

void workqueue_add(struct workqueue *wq, void (*func)(void *), void *arg) {
  return workqueue_add_delayed(wq, func, arg, 0);
}


