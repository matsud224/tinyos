#pragma once

#include <stdint.h>

void pic_init(void);
void pic_sendeoi(void);

void pic_setmask(int irq);
void pic_clearmask(int irq);

