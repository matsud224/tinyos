[bits 32]

section .text

global out8
out8:
  push ebp
  mov ebp, esp
  mov dx, [ebp+4]
  mov al, [ebp+8]
  out dx, al
  pop ebp
  ret

global out16 
out16:
  push ebp
  mov ebp, esp
  mov dx, [ebp+4]
  mov ax, [ebp+8]
  out dx, ax
  pop ebp
  ret

global out32
out32:
  push ebp
  mov ebp, esp
  mov dx, [ebp+4]
  mov eax, [ebp+8]
  out dx, eax
  pop ebp
  ret


global lidt
global lidt
lidt:
  push ebp
  mov ebp, esp
  mov ebx, [ebp+4]
  lidt [ebx]
  pop ebp
  ret

