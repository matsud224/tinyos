[bits 16]

mapbase					equ 0x500
kernbufbase				equ	0x1000
pdtbase					equ 0x2000
kernbase				equ	0x7e00
virtkernbase		equ 0xc0007e00
kernsectors			equ 30
codeseg					equ	0x8
dataseg					equ	0x10

secpertrk:			dw 0x0000
nheads:					dw 0x0000

org 0x7c00
boot:
  cli
  ; initialize segment registers
  xor ax, ax
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax

  ;prepare stack
  mov ss, ax
  mov sp, 0x7bff

  mov si, hellomsg
  call putstr

  ;get memory map
  mov di, mapbase
  xor ebx, ebx
.nextent:
  mov eax, 0xe820
  mov ecx, 20
  mov edx, 'PAMS'
  int 0x15
  jc .maperror
  cmp eax, 'PAMS'
  jne .maperror
  cmp ebx, 0
  jz .mapfin
  add di, 20
  jmp .nextent
.maperror:
  hlt
.mapfin:
  mov dword [di], 0
  mov dword [di+8], 0
 
  ;reset floppy
  xor ah, ah
  xor dl, dl
  int 0x13
  jc floppyerr

  ;get drive parameter
  mov ah, 0x08
  xor dl, dl
  int 0x13
  jc floppyerr
  mov [nheads], dh
  mov al, cl
  and al, 0x3f
  mov [secpertrk], al

.readkern:
  ;read kernel to kernbase
  mov bp, 0x1
  mov ax, kernbase/0x10
  mov es, ax
.readloop:
  mov ax, bp
  call lba2chs
  push es
  mov ax, 0
  mov es, ax
  mov ah, 0x02
  mov al, 0x01
  mov ch, byte [ptrack]
  mov cl, byte [psector]
  mov dh, byte [phead]
  xor dl, dl
  mov bx, kernbufbase
  int 0x13
  jc floppyerr
  pop es
  call copy
  mov ax, es
  add ax, 0x20
  mov es, ax
  cmp bp, kernsectors
  jz setuppdt
  inc bp
  jmp .readloop
floppyerr:
  hlt

copy:
  cld
  mov si, kernbufbase
  xor di, di
  mov cx, 0x200
  rep movsb
  ret

setuppdt:
  xor ax, ax
  mov es, ax
  ;create 4MB page directory table
  ;clear
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

  ;setup gdt
  lgdt [gdtptr]
  
  ;enable a20
  call waitkbdin
  mov al, 0xad
  out 0x64, al
  call waitkbdin
  mov al, 0xd0
  out 0x64, al
  call waitkbdout
  in al, 0x60
  or al, 0x2
  push ax
  call waitkbdin
  mov al, 0xd1
  out 0x64, al
  call waitkbdin
  pop ax
  out 0x60, al
  call waitkbdin
  mov al, 0xae
  out 0x64, al
  call waitkbdin

  ;to protect mode
  mov eax, cr0
  or eax, 1
  mov cr0, eax
  jmp .flush
.flush:
  ;jump to 32bit segment
  jmp codeseg:kernstart

hellomsg:
db "...", 0x0a, 0x0d, 0x00

gdtptr:
  dw 8*3-1
  dd gdt

gdtptr_virtaddr:
  dw 8*3-1
  dd gdt+0xc0000000

gdt:
  db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 					;null
  db 0xff, 0xff, 0x00, 0x00, 0x00, 10011010b, 11001111b, 0x00 ;code segment
  db 0xff, 0xff, 0x00, 0x00, 0x00, 10010010b, 11001111b, 0x00 ;data segment


putstr:
  push ax
  push bx
.putone:
  lodsb
  or al, al
  jz .done
  mov ah, 0x0e
  mov bh, 0x00
  mov bl, 0x07
  int 0x10
  jmp .putone
.done:
  pop bx
  pop ax
  ret

waitkbdin:
  in al, 0x64
  test al, 0x2
  jnz waitkbdin
  ret

waitkbdout:
  in al, 0x64
  test al, 0x1
  jz waitkbdout
  ret

psector:	db 0x00
phead:		db 0x00
ptrack:		db 0x00

lba2chs:
  xor dx, dx
  div word [secpertrk]
  inc dl
  mov byte [psector], dl
  xor dx, dx
  div word [nheads]
  mov byte [phead], dl
  mov byte [ptrack], al
  ret

[bits 32]
kernstart:
  ;init segment registers
  cli
  mov ax, dataseg
  mov ss, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ds, ax
  mov esp, 0xc0007bff
  ; enable paging
  mov eax, cr0
  or eax, 0x80000000
  mov cr0, eax
  jmp .flush2
.flush2:
  lgdt [gdtptr_virtaddr]
  call virtkernbase
.loop:
  hlt
  jmp .loop

times 510 - ($ - $$) db 0
dw 0xaa55
