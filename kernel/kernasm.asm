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
  push ebx
  mov bx, 0x80
  mov ds, bx
  mov ax, [ecx]
  pop ebx
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
  mov eax, [ebp+8]
  lidt [eax]
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

global getcr2
getcr2:
  mov eax, cr2
  ret

extern current_pdt
global flushtlb
flushtlb:
  mov eax, [current_pdt]
  sub eax, 0xc0000000
  mov cr3, eax
  jmp .flush
.flush:
  ret

global a20_enable
a20_enable:
  call .waitkbdin
  mov al, 0xad
  out 0x64, al
  call .waitkbdin
  mov al, 0xd0
  out 0x64, al
  call .waitkbdout
  in al, 0x60
  or al, 0x2
  push ax
  call .waitkbdin
  mov al, 0xd1
  out 0x64, al
  call .waitkbdin
  pop ax
  out 0x60, al
  call .waitkbdin
  mov al, 0xae
  out 0x64, al
  call .waitkbdin
  ret
 
.waitkbdin:
  in al, 0x64
  test al, 0x2
  jnz .waitkbdin
  ret

.waitkbdout:
  in al, 0x64
  test al, 0x1
  jz .waitkbdout
  ret

extern current_task
global saveregs
saveregs:
  ; スタックには旧eip,cs,esp,ssが積まれている
  mov [current_task], eax
  mov [current_task+4], ecx
  mov [current_task+8], edx
  mov [current_task+12], ebx
  mov [current_task+16], ebp
  mov [current_task+20], esi
  mov [current_task+24], edi
  mov [current_task+28], es
  mov [current_task+40], ds
  mov [current_task+44], fs
  mov [current_task+48], gs
  pushfd
  mov eax, [esp]
  mov [current_task+52], eax
  mov eax, [esp+4]
  mov [current_task+56], eax
  mov eax, [esp+8]
  mov [current_task+60], eax
  mov eax, [esp+12]
  mov [current_task+64], eax
  mov eax, [esp+16]
  mov [current_task+68], eax
  ; 復帰時に積み直すのでpop
  add esp, 20
  ret
  
global taskswitch
taskswitch:
  mov eax, [current_task]
  mov ecx, [current_task+4]
  mov edx, [current_task+8]
  mov ebx, [current_task+12]
  mov ebp, [current_task+16]
  mov esi, [current_task+20]
  mov edi, [current_task+24]
  mov es, [current_task+28]
  mov ds, [current_task+40]
  mov fs, [current_task+44]
  mov gs, [current_task+48]
  mov ax, [current_task+68]
  push ax
  mov eax, [current_task+64]
  push eax
  mov ax, [current_task+60]
  push ax
  mov eax, [current_task+56]
  push eax
  mov eax, [current_task+52]
  push eax
  popfd
  iretd

