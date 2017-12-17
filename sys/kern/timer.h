#pragma once
#include <kern/kernlib.h>
#include <kern/pit.h>

#define HZ PIT_HZ
#define msecs_to_ticks(msec) ((msec) * HZ / 1000)

struct timer_entry;

void timer_start(u32 ticks, void (*func)(void *), void *arg);
void timer_tick(void);
void *timer_getarg(struct timer_entry *t);
