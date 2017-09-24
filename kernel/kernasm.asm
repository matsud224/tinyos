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

global lgdt
lgdt:
  push ebp
  mov ebp, esp
  mov eax, [ebp+8]
  lgdt [eax]
  pop ebp
  ret

global ltr
ltr:
  push ebp
  mov ebp, esp
  ltr word [ebp+8]
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

extern current

global saveregs
saveregs:
  push eax
  mov eax, [current]
  mov [eax+4], ecx
  mov [eax+8], edx
  mov [eax+12], ebx
  mov [eax+16], ebp
  mov [eax+20], esi
  mov [eax+24], edi
  mov [eax+28], es
  mov [eax+32], ds
  mov [eax+36], fs
  mov [eax+40], gs
  mov ecx, eax
  pop eax
  mov [ecx], eax 
  pushfd
  pop eax
  mov [ecx+44], eax 
  ret
 
global saveregs_intr
saveregs_intr:
  push eax
  mov eax, [current]
  mov [eax+4], ecx
  mov [eax+8], edx
  mov [eax+12], ebx
  mov [eax+16], ebp
  mov [eax+20], esi
  mov [eax+24], edi
  mov [eax+28], es
  mov [eax+32], ds
  mov [eax+36], fs
  mov [eax+40], gs
  mov ecx, eax
  pop eax
  mov [ecx], eax 
  pushfd
  pop eax
  or eax, 0x200 ;set IF
  mov [ecx+44], eax 
  mov eax, [esp+4]
  mov [ecx+48], eax
  mov eax, [esp+8]
  mov [ecx+52], eax
  and eax, 0x3 
  cmp eax, 0x3 ;check privilege level
  jne .kernmode
  mov eax, [esp+12]
  mov [ecx+56], eax
  mov eax, [esp+16]
  mov [ecx+60], eax
  jmp .fin
.kernmode:
  mov eax, esp
  add eax, 16
  mov [ecx+56], eax
  mov [ecx+60], ss
.fin:
  ret
  
global rettotask
rettotask:
  mov eax, [current]
  mov ecx, [eax+4]
  mov edx, [eax+8]
  mov ebx, [eax+12]
  mov ebp, [eax+16]
  mov esi, [eax+20]
  mov edi, [eax+24]
  mov es, [eax+28]
  mov ds, [eax+32]
  mov fs, [eax+36]
  mov gs, [eax+40]
  mov ss, [eax+60]
  mov esp, [eax+56]
  push dword [eax+44]
  push dword [eax+52]
  push dword [eax+48]
  mov eax, [eax+0]
  iretd


extern task_sched
extern kernstack_setaddr

global task_yield
task_yield:
  call saveregs
  mov eax, [current]
  mov dword [eax+48], .resume
  mov [eax+52], cs
  mov [eax+56], esp
  mov [eax+60], ss
  call task_sched
  call kernstack_setaddr
  jmp rettotask
.resume:
  ret

global cpu_halt
cpu_halt:
  hlt
  ret
