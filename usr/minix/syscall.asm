[bits 32]

section .text

%macro systemcall 0
  int 0x80
%endmacro

global do_cli
do_cli:
  cli
  ret

global syscall_0
syscall_0:
  mov eax, [esp+4]
  systemcall
  ret

global syscall_1
syscall_1:
  push ebx
  mov eax, [esp+8]
  mov ebx, [esp+12]
  systemcall
  pop ebx
  ret

global syscall_2
syscall_2:
  push ebx
  mov eax, [esp+8]
  mov ebx, [esp+12]
  mov ecx, [esp+16]
  systemcall
  pop ebx
  ret

global syscall_3
syscall_3:
  push ebx
  mov eax, [esp+8]
  mov ebx, [esp+12]
  mov ecx, [esp+16]
  mov edx, [esp+20]
  systemcall
  pop ebx
  ret

global syscall_4
syscall_4:
  push ebx
  push esi
  mov eax, [esp+12]
  mov ebx, [esp+16]
  mov ecx, [esp+20]
  mov edx, [esp+24]
  mov esi, [esp+28]
  systemcall
  pop esi
  pop ebx
  ret

global syscall_5
syscall_5:
  push ebx
  push esi
  push edi
  mov eax, [esp+16]
  mov ebx, [esp+20]
  mov ecx, [esp+24]
  mov edx, [esp+28]
  mov esi, [esp+32]
  mov edi, [esp+36]
  systemcall
  pop edi
  pop esi
  pop ebx
  ret

