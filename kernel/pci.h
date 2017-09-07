#pragma once

#include "pci.h"
#include <stdint.h>
#include <stddef.h>

uint32_t pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint16_t pci_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
uint8_t pci_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset);
void pci_printinfo(void);
 
