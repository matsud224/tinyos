[bits 32]

section .text

extern pit_inthandler
global pit_isr
pit_isr:
  pushad
  cld
  call pit_inthandler
  popad
  iretd

extern gpe_inthandler
global gpe_isr
gpe_isr:
  pushad
  cld
  call gpe_inthandler
  popad
  iretd

