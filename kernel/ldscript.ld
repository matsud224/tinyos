SECTIONS
{
  . = 0x7e00;
  .entry :	{ *(.entry) }
  .text :		{ *(.text)  }
  .rodata :	{ *(.rodata)  }
  .data	:		{ *(.data) }
  .bss :		{ *(.bss)  }
}
