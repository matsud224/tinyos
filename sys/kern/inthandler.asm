[bits 32]

section .text

extern kernstack_setaddr
extern task_sched

%macro handler_enter 0
  push eax
  push ecx
  push edx
%endmacro

%macro handler_leave 0
  pop edx
  pop ecx
  pop eax
  iretd
%endmacro

extern pit_isr
global pit_inthandler
pit_inthandler:
  handler_enter
  call pit_isr
  handler_leave

extern ide1_isr
global ide1_inthandler
ide1_inthandler:
  handler_enter
  call ide1_isr
  handler_leave

extern ide2_isr
global ide2_inthandler
ide2_inthandler:
  handler_enter
  call ide2_isr
  handler_leave

extern com1_isr
global com1_inthandler
com1_inthandler:
  handler_enter
  call com1_isr
  handler_leave

extern com2_isr
global com2_inthandler
com2_inthandler:
  handler_enter
  call com2_isr
  handler_leave

extern rtl8139_isr
global rtl8139_inthandler
rtl8139_inthandler:
  handler_enter
  call rtl8139_isr
  handler_leave

extern gpe_isr
global gpe_inthandler
gpe_inthandler:
  add esp, 4 ; pop error code
  handler_enter
  call gpe_isr
  handler_leave

extern pf_isr
global pf_inthandler
pf_inthandler:
  add esp, 4 ; pop error code
  handler_enter
  mov ecx, esp
  mov eax, [ecx+12] ;saved eip
  push eax
  mov eax, cr2
  push eax
  call pf_isr
  add esp, 8
  handler_leave

extern syscall_isr
global syscall_inthandler
syscall_inthandler:
  handler_enter
  push edi
  push esi
  push edx
  push ecx
  push ebx
  push eax
  call syscall_isr
  add esp, 24
  handler_leave
  
