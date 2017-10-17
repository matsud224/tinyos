#pragma once
#include "kernlib.h"
#include "pit.h"

#define SEC PIT_HZ

void timer_start(u32 expire, void (*func)(void *), void *arg);
void timer_tick(void);
