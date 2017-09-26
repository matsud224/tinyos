#pragma once

#include <stdint.h>
#include <stddef.h>

typedef uint32_t mutex;

void mutex_init(mutex *mtx);
void mutex_lock(mutex *mtx);
int mutex_trylock(mutex *mtx);
void mutex_unlock(mutex *mtx);
