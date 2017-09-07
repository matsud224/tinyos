#pragma once

#include "ide.h"
#include <stdint.h>
#include <stddef.h>

void ide_init(void);
int ide_ata_access(uint8_t dir, uint8_t drv, uint32_t lba, uint8_t nsect, uint16_t sel, uint32_t edi);
void ide1_isr(void);
void ide2_isr(void);
void ide1_inthandler(void);
void ide2_inthandler(void);

