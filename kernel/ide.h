#pragma once

#include "ide.h"
#include <stdint.h>
#include <stddef.h>

struct request;

void ide_init(void);
void *ide_request(struct request *req);
void ide1_isr(void);
void ide2_isr(void);
void ide1_inthandler(void);
void ide2_inthandler(void);

