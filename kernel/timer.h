#pragma once
#include "kernlib.h"

void timer_start(u32 expire, void (*func)(void *), void *arg);
void timer_tick(void);
