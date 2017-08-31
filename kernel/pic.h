#pragma once

#include <stdint.h>

void pic_init(void);
void pic_sendeoi(void);

void pic_setmask_master(int irq);
void pic_setmask_slave(int irq);
void pic_clearmask_master(int irq);
void pic_clearmask_slave(int irq);

