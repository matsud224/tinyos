#include <kern/kernlib.h>

struct workqueue;

struct workqueue *workqueue_new(void);
void workqueue_add_delayed(struct workqueue *wq, void (*func)(void *), void *arg, int ticks);
void workqueue_add(struct workqueue *wq, void (*func)(void *), void *arg);

