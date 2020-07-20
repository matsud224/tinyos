ENTRY(_start)

MEMORY
{
  protmem(rwxa) : ORIGIN = 0x100000, LENGTH = 100M
  kernvirt(rwx) : ORIGIN = 0xc0100000, LENGTH = 100M
}

SECTIONS
{
  . = 0x100000;
  .startup BLOCK(4K) : ALIGN(4K)
  {
    *(.multiboot)
    *(.startup)
  } > protmem AT> protmem

  . = . + 0xc0000000;
  .text BLOCK(4K) : ALIGN(4K)
  {
    *(.stack)
    *(.text)
    *(.text.*)
  } > kernvirt AT> protmem

  .rodata BLOCK(4K) : ALIGN(4K)
  {
    *(.rodata)
    *(.rodata.*)
  } > kernvirt AT> protmem

  .data BLOCK(4K) : ALIGN(4K)
  {
    *(.data)
  } > kernvirt AT> protmem

  .bss BLOCK(4K) : ALIGN(4K)
  {
    *(.bss)
  } > kernvirt AT> protmem

  _kernel_end = .;
}
