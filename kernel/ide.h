#pragma once

#include "ide.h"
#include <stdint.h>
#include <stddef.h>

struct io_request;

void ide_init(void);
struct io_request *ide_request(uint8_t drvno, uint64_t blockno, uint16_t nsect, void *paddr, uint32_t flags);
void ioreq_wait(struct io_request *req);
int ioreq_checkerror(struct io_request *req);
void ide1_isr(void);
void ide2_isr(void);
void ide1_inthandler(void);
void ide2_inthandler(void);

