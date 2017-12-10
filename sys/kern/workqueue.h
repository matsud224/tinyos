#include <kern/kernlib.h>

struct workqueue;

struct workqueue *workqueue_new(char *name);
void workqueue_add_delayed(struct workqueue *wq, void (*func)(void *), void *arg, int delay);
void workqueue_add(struct workqueue *wq, void (*func)(void *), void *arg);

