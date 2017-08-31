SECTIONS
{
  . = 0xc0007e00;
  .entry :	{ *(.entry) }
  .text :		{ *(.text)  }
  .rodata :	{ *(.rodata)  }
  .data	:		{ *(.data) }
  .bss :		{ *(.bss)  }
}
