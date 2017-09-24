[bits 32]

section .text

extern saveregs_intr
extern rettotask
extern kernstack_setaddr
extern task_sched

%macro handler_enter 0
  call saveregs_intr
%endmacro

%macro handler_leave 0
  call task_sched
  call kernstack_setaddr
  jmp rettotask
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
  handler_enter
  call gpe_isr
  handler_leave

extern pf_isr
global pf_inthandler
pf_inthandler:
  handler_enter
  mov ecx, dword [esp+32]
  mov eax, cr2
  push eax
  push ecx
  call pf_isr
  add esp, 8
  add esp, 4 ; pop error code
  handler_leave

extern syscall_isr
global syscall_inthandler
syscall_inthandler:
  handler_enter
  call syscall_isr
  handler_leave
  
