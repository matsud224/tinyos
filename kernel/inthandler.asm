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
  mov eax, cr2
  push eax
  call pf_isr
  add esp, 4
  handler_leave

extern syscall_isr
global syscall_inthandler
syscall_inthandler:
  handler_enter
  call syscall_isr
  handler_leave
  
