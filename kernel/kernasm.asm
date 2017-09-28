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

extern current
global flushtlb
flushtlb:
  mov eax, [esp+4]
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

global saveesp
saveesp:
  mov eax, [current]
  mov ecx, esp
  sub ecx, 20
  mov [eax], ecx
  ret

extern task_sched
extern kernstack_setaddr

global task_yield
task_yield:
;save esp
  mov eax, [current]
  mov ecx, esp
  sub ecx, 20
  mov [eax], ecx
;call scheduler
  call task_sched
  call kernstack_setaddr
;save registers
  push ebp
  push ebx
  push esi
  push edi
  pushfd
;switch stack
  mov eax, [current]
  mov esp, [eax] ;new esp
  mov ecx, [eax+4] ;new cr3
  sub ecx, 0xc0000000
  mov cr3, ecx
  popfd
  pop edi
  pop esi
  pop ebx
  pop ebp
  ret

global cpu_halt
cpu_halt:
  hlt
  ret

global xchg
xchg:
  mov eax, [esp+4]
  mov ecx, [esp+8]
  xchg eax, [ecx]
  ret

global jmpto_current
jmpto_current:
  mov eax, [current]
  mov esp, [eax]
  mov ecx, [eax+4]
  sub ecx, 0xc0000000
  mov cr3, ecx
  popfd
  pop edi
  pop esi
  pop ebx
  pop ebp
  ret

