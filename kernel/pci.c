#include "pci.h"
#include "kernasm.h"
#include "kernlib.h"
#include "list.h"
#include <stdint.h>
#include <stddef.h>

#define PCI_CONFIG_ADDR 0xcf8
#define PCI_CONFIG_DATA 0xcfc

static struct list_head pci_dev_list;

static u32 _pci_config_read32(u8 bus, u8 dev, u8 func, u8 offset) {
  u32 addr = (bus<<16) | (dev<<11) | (func<<8) | (offset&0xfc) | 0x80000000u;
  out32(PCI_CONFIG_ADDR, addr);
  return in32(PCI_CONFIG_DATA);
}

u32 pci_config_read32(struct pci_dev *pcidev, u8 offset) {
  return _pci_config_read32(pcidev->bus, pcidev->dev, pcidev->func, offset);
}

static u16 _pci_config_read16(u8 bus, u8 dev, u8 func, u8 offset) {
  u32 result = _pci_config_read32(bus, dev, func, offset);
  return (result >> ((offset&2)*8)) & 0xffff;
}

u16 pci_config_read16(struct pci_dev *pcidev, u8 offset) {
  return _pci_config_read16(pcidev->bus, pcidev->dev, pcidev->func, offset);
}

static u8 _pci_config_read8(u8 bus, u8 dev, u8 func, u8 offset) {
  u32 result = _pci_config_read32(bus, dev, func, offset);
  return (result >> ((offset&3)*8)) & 0xff;
}

u8 pci_config_read8(struct pci_dev *pcidev, u8 offset) {
  return _pci_config_read8(pcidev->bus, pcidev->dev, pcidev->func, offset);
}

static void _pci_config_write32(u8 bus, u8 dev, u8 func, u8 offset, u32 data) {
  u32 addr = (bus<<16) | (dev<<11) | (func<<8) | (offset&0xfc) | 0x80000000u;
  out32(PCI_CONFIG_ADDR, addr);
  out32(PCI_CONFIG_DATA, data);
}

void pci_config_write32(struct pci_dev *pcidev, u8 offset, u32 data) {
  return _pci_config_write32(pcidev->bus, pcidev->dev, pcidev->func, offset, data);
}

static u16 _pci_config_write16(u8 bus, u8 dev, u8 func, u8 offset, u16 data) {
  u32 result = _pci_config_read32(bus, dev, func, offset);
  result &= (~0xffff << ((offset&2)*8));
  result &= (data << ((offset&2)*8));
  _pci_config_write32(bus, dev, func, offset, result);
}

u16 pci_config_write16(struct pci_dev *pcidev, u8 offset, u16 data) {
  return _pci_config_write16(pcidev->bus, pcidev->dev, pcidev->func, offset, data);
}

static u8 _pci_config_write8(u8 bus, u8 dev, u8 func, u8 offset, u8 data) {
  u32 result = _pci_config_read32(bus, dev, func, offset);
  result &= (~0xff << ((offset&3)*8));
  result &= (data << ((offset&3)*8));
  _pci_config_write32(bus, dev, func, offset, result);
}

u8 pci_config_write8(struct pci_dev *pcidev, u8 offset, u8 data) {
  return _pci_config_write8(pcidev->bus, pcidev->dev, pcidev->func, offset, data);
}

struct pci_dev *pci_search_device(u16 vendorid, u16 deviceid) {
  struct list_head *ptr;
  list_foreach(ptr, &pci_dev_list) {
    struct pci_dev *pcidev = container_of(ptr, struct pci_dev, link);
    if(pcidev->vendorid == vendorid && pcidev->deviceid == deviceid)
      return pcidev;
  }
  return NULL;
}

static void pci_dev_add(u8 bus, u8 dev, u8 func) {
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
          for(u8 func = 1; func < 8; func++)
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
