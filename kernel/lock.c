#include "lock.h"
#include "kernasm.h"
#include "task.h"
#include <stdint.h>
#include <stddef.h>

void mutex_init(mutex *mtx) {
  *mtx = 0;
}

void mutex_lock(mutex *mtx) {
  cli();
  while(xchg(1, mtx))
    task_sleep(mtx);
  sti();
}

int mutex_trylock(mutex *mtx) {
  if(xchg(1, mtx) == 0)
    return 0;
  else
    return -1;
}

void mutex_unlock(mutex *mtx) {
  xchg(0, mtx);
  task_wakeup(mtx);
}
