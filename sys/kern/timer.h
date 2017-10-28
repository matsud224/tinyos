#pragma once
#include <kern/kernlib.h>
#include <kern/pit.h>

#define SEC PIT_HZ

void timer_start(u32 expire, void (*func)(void *), void *arg);
void timer_tick(void);
