#pragma once

#include "pci.h"
#include "list.h"
#include <stdint.h>
#include <stddef.h>

struct pci_dev {
  struct list_head link;
  uint8_t bus;
  uint8_t dev;
  uint8_t func;
  uint16_t vendorid;
  uint16_t deviceid;
  uint8_t revid;
  uint8_t classcode;
  uint8_t hdrtype; 
};

#define PCI_DEVICEID			0x2
#define PCI_VENDORID			0x0
#define PCI_STATUS				0x6
#define PCI_COMMAND				0x4
#define PCI_CLASS					0xb
#define PCI_SUBCLASS			0xa
#define PCI_PROGIF				0x9
#define PCI_REVID					0x8
#define PCI_BIST					0xf
#define PCI_HEADERTYPE		0xe
#define PCI_LATAENCYTIMER	0xd
#define PCI_CACHELINESIZE	0xc
#define PCI_BAR0					0x10
#define PCI_BAR1					0x14
#define PCI_BAR2					0x18
#define PCI_BAR3					0x1c
#define PCI_BAR4					0x20
#define PCI_BAR5					0x24
#define PCI_INTLINE				0x3c

static const char *PCI_CLASS_STR[0x12] = {
  "Device was built prior definition of the class code field",
  "Mass Storage Controller",
  "Network Controller",
  "Display Controller",
  "Multimedia Controller",
  "Memory Controller",
  "Bridge Device",
  "Simple Communication Controllers",
  "Base System Peripherals",
  "Input Devices",
  "Docking Stations",
  "Processors",
  "Serial Bus Controllers",
  "Wireless Controllers",
  "Intelligent I/O Controllers",
  "Satellite Communication Controllers",
  "Encryption/Decryption Controllers",
  "Data Acquisition and Signal Processing Controllers",
};


uint32_t pci_config_read32(struct pci_dev *pcidev, uint8_t offset);
uint16_t pci_config_read16(struct pci_dev *pcidev, uint8_t offset);
uint8_t pci_config_read8(struct pci_dev *pcidev, uint8_t offset);
void pci_config_write32(struct pci_dev *pcidev, uint8_t offset, uint32_t data);
uint16_t pci_config_write16(struct pci_dev *pcidev, uint8_t offset, uint16_t data);
uint8_t pci_config_write8(struct pci_dev *pcidev, uint8_t offset, uint8_t data);
struct pci_dev *pci_search_device(uint16_t vendorid, uint16_t deviceid);
void pci_init(void);
 
