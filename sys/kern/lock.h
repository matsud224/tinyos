#pragma once
#include <kern/types.h>

typedef u32 mutex;

void mutex_init(mutex *mtx);
void mutex_lock(mutex *mtx);
int mutex_trylock(mutex *mtx);
void mutex_unlock(mutex *mtx);
