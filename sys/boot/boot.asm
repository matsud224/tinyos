[bits 32]

pdtbase      equ 0x2000
kernstacksz  equ 4096 * 8

mb_magic     equ 0x1badb002
mb_flags     equ 0x3
mb_checksum  equ -(mb_magic + mb_flags)

section .multiboot
align 4
dd mb_magic
dd mb_flags
dd mb_checksum

section .stack
align 16
stack_bottom:
times kernstacksz db 0
stack_top:

section .startup
extern kernel_main
global _start
_start:
  mov esp, 0x7bff
  push ebx ; argument for kernel_main

setuppdt:
  xor ax, ax
  mov es, ax
  ; create 4MB page directory table
  ; clear
  cld
  mov di, pdtbase
  xor ax, ax
  mov cx, 1024*4/2
  rep stosw
  ; add mapping
  mov eax, pdtbase+(0x300*4) ; from 0xc0000000
  mov ebx, 0x83 ; P,RW,PS bit
  mov ecx, 224
.nextdent:
  mov [eax], ebx
  add eax, 4
  add ebx, 0x400000
  dec ecx
  jnz .nextdent
  ; add mapping
  mov eax, pdtbase
  mov ebx, 0x83
  mov [eax], ebx
  ; prepare paging related registers
  mov eax, pdtbase
  mov cr3, eax
  mov eax, cr4
  or eax, 0x10
  mov cr4, eax
  ; prepare new stack
  pop ebx
  mov esp, stack_top
  ; enable paging
  mov eax, cr0
  or eax, 0x80000000
  mov cr0, eax
  jmp .flush2
.flush2:

  push ebx
  call kernel_main
  cli
.loop:
  hlt
  jmp .loop
