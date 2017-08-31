[bits 32]

section .text

global divzero
divzero:
  mov eax, 1
  xor ecx, ecx
  div ecx
  ret

global gengpe
gengpe:
  mov bx, 0x80
  mov ds, [bx]
  mov ax, [ecx]
  ret

global out8
out8:
  push ebp
  mov ebp, esp
  mov dx, [ebp+8]
  mov al, [ebp+12]
  out dx, al
  pop ebp
  ret

global out16 
out16:
  push ebp
  mov ebp, esp
  mov dx, [ebp+8]
  mov ax, [ebp+12]
  out dx, ax
  pop ebp
  ret

global out32
out32:
  push ebp
  mov ebp, esp
  mov dx, [ebp+8]
  mov eax, [ebp+12]
  out dx, eax
  pop ebp
  ret

global in8
in8:
  push ebp
  mov ebp, esp
  mov dx, [ebp+8]
  xor eax, eax
  in al, dx
  pop ebp
  ret

global in16
in16:
  push ebp
  mov ebp, esp
  mov dx, [ebp+8]
  xor eax, eax
  in ax, dx
  pop ebp
  ret

global in32
in32:
  push ebp
  mov ebp, esp
  mov dx, [ebp+8]
  xor eax, eax
  in eax, dx
  pop ebp
  ret

global lidt
lidt:
  push ebp
  mov ebp, esp
  mov ebx, [ebp+8]
  lidt [ebx]
  pop ebp
  ret

global sti
sti:
  sti
  ret

global cli
cli:
  cli
  ret
