#include "pci.h"
#include "kernasm.h"
#include "kernlib.h"
#include "list.h"
#include <stdint.h>
#include <stddef.h>

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

static struct list_head pci_dev_list;

static uint32_t _pci_config_read32(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
  uint32_t addr = (bus<<16) | (dev<<11) | (func<<8) | (offset&0xfc) | 0x80000000u;
  out32(PCI_CONFIG_ADDR, addr);
  return in32(PCI_CONFIG_DATA);
}

uint32_t pci_config_read32(struct pci_dev *pcidev, uint8_t offset) {
  return _pci_config_read32(pcidev->bus, pcidev->dev, pcidev->func, offset);
}

static uint16_t _pci_config_read16(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
  uint32_t result = _pci_config_read32(bus, dev, func, offset);
  return (result >> ((offset&2)*8)) & 0xffff;
}

uint16_t pci_config_read16(struct pci_dev *pcidev, uint8_t offset) {
  return _pci_config_read16(pcidev->bus, pcidev->dev, pcidev->func, offset);
}

static uint8_t _pci_config_read8(uint8_t bus, uint8_t dev, uint8_t func, uint8_t offset) {
  uint32_t result = _pci_config_read32(bus, dev, func, offset);
  return (result >> ((offset&3)*8)) & 0xff;
}

uint8_t pci_config_read8(struct pci_dev *pcidev, uint8_t offset) {
  return _pci_config_read8(pcidev->bus, pcidev->dev, pcidev->func, offset);
}

struct pci_dev *pci_search_device(uint16_t vendorid, uint16_t deviceid) {
  struct list_head *ptr;
  list_foreach(ptr, &pci_dev_list) {
    struct pci_dev *pcidev = container_of(ptr, struct pci_dev, link);
    if(pcidev->vendorid == vendorid && pcidev->deviceid == deviceid)
      return pcidev;
  }
  return NULL;
}

static void pci_dev_add(uint8_t bus, uint8_t dev, uint8_t func) {
  struct pci_dev *pcidev = malloc(sizeof(struct pci_dev));
  pcidev->bus = bus;
  pcidev->dev = dev;
  pcidev->func = func;
  pcidev->vendorid = _pci_config_read16(bus, dev, func, PCI_VENDORID);
  pcidev->deviceid = _pci_config_read16(bus, dev, func, PCI_DEVICEID);
  pcidev->revid = _pci_config_read8(bus, dev, func, PCI_REVID);
  pcidev->classcode = _pci_config_read8(bus, dev, func, PCI_CLASS);
  pcidev->hdrtype = _pci_config_read8(bus, dev, func, PCI_HEADERTYPE);
  printf("%x:%x:%x vendorid:%x deviceid:%x %s\n", bus, dev, func, pcidev->vendorid, pcidev->deviceid, 
    pcidev->classcode>=0x12?"Unknown":PCI_CLASS_STR[pcidev->classcode]);
  list_pushback(&pcidev->link, &pci_dev_list);
}

static void pci_enumerate() {
  list_init(&pci_dev_list);
  for(int bus = 0; bus < 256; bus++) {
    for(int dev = 0; dev < 32; dev++) {
      if(_pci_config_read16(bus, dev, 0, PCI_VENDORID) != 0xffff) {
        pci_dev_add(bus, dev, 0);
        if(_pci_config_read8(bus, dev, 0, PCI_HEADERTYPE) & 0x80) {
          for(uint8_t func = 1; func < 8; func++)
            if(_pci_config_read16(bus, dev, func, PCI_VENDORID) != 0xffff)
              pci_dev_add(bus, dev, func);
        }
      }
    }
  }
}

void pci_init() {
  pci_enumerate();
}
