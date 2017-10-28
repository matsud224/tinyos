#include <kern/idt.h>
#include <kern/params.h>
#include <kern/kernasm.h>

#define IDT_PRESENT 0x80
#define IDT_GATE32 0x8

#define IDTSIZE 256

static struct gatedesc {
  u16 baselo;
  u16 selector;
  u8 reserved;
  u8 flags;
  u16 basehi;
} PACKED idt[IDTSIZE];

static struct idtr {
  u16 limit;
  struct gatedesc *base;
} PACKED idtr; 


void idt_register(u8 vecnum, u8 gatetype, void (*base)(void)) {
  struct gatedesc *desc = &idt[vecnum];
  desc->selector = GDT_SEL_CODESEG_0;
  desc->baselo = (u32)base & 0xffff;
  desc->basehi = (u32)base >> 16;
  desc->reserved = 0;
  desc->flags = gatetype | IDT_PRESENT | IDT_GATE32;
}

void idt_unregister(u8 vecnum) {
  idt[vecnum].flags = 0;
}

void idt_init() {
  for(int i=0; i<IDTSIZE; i++)
    idt_unregister(i);
  
  idtr.limit = IDTSIZE * sizeof(struct gatedesc);
  idtr.base = idt;
  lidt(&idtr);
}

