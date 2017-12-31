[bits 32]

section .text

%macro systemcall 0
  int 0x80
%endmacro

global do_cli
do_cli:
cli
ret

global funcall
funcall:
ret

global exit
exit:
mov eax, 0x0
systemcall
ret

global syscall1
syscall1:
mov eax, 0x1
systemcall
ret

global syscall2
syscall2:
mov eax, 0x2
systemcall
ret

