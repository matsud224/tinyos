[bits 16]

mapbase equ 		0x500
pdtbase equ 0x1000
kernbase equ 		0x7e00
virtkernbase equ 0xc0007e00
kernsectors equ 20
codeseg equ		 	0x8
dataseg equ 		0x10

kerntail:				dw 0x0000
secpertrk:			dw 0x0012
nheads:					dw 0x0002

org 0x7c00
jmp boot

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
  mov sp, 0xffff

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
  mov si, maperrmsg
  call putstr
  hlt
.mapfin:
  mov dword [di], 0
  mov dword [di+8], 0
 
  ;reset floppy
  mov ah, 0x00
  mov dl, 0x00
  int 0x13
  jc floppyerr

.readkern:
  ;read kernel to kernbase
  mov si, 0x1
  mov ax, kernbase
  mov [kerntail], ax
.readloop:
  mov ax, si
  call lba2chs
  mov ah, 0x02
  mov al, 0x01
  mov ch, byte [ptrack]
  mov cl, byte [psector]
  mov dh, byte [phead]
  mov dl, 0x00
  mov bx, [kerntail]
  int 0x13
  jc floppyerr
  mov ax, [kerntail]
  add ax, 512
  mov [kerntail], ax
  cmp si, kernsectors
  jz setuppdt
  inc si
  jmp .readloop
floppyerr:
  mov si, floppyerrmsg
  call putstr
  hlt

setuppdt:
  ;create 4MB page directory table
  ;clear
  cld
  mov di, pdtbase
  mov ax, 0
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
  hlt

hellomsg:
db "Loading kernel...", 0x0a, 0x0d, 0x00

floppyerrmsg:
db "floppy error", 0x0a, 0x0d, 0x00

maperrmsg:
db "memmap error", 0x0a, 0x0d, 0x00


gdtptr:
  dw 8*3-1
  dd gdt

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

psector:	dd 0x00
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
  mov esp, 0x7bff
  ; enable paging
  mov eax, cr0
  or eax, 0x80000000
  mov cr0, eax
  jmp .flush2
.flush2:
  call kernbase
.loop:
  hlt
  jmp .loop

times 510 - ($ - $$) db 0
dw 0xaa55
