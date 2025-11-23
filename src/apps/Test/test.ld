ENTRY(_start)
PHDRS { text PT_LOAD FLAGS(5); }
SECTIONS {
  . = 0x00100000;
  .text ALIGN(16) : { *(.text*) } :text
  .rodata ALIGN(16) : { *(.rodata*) } :text
  .data ALIGN(16) : { *(.data*) } :text
  .bss  ALIGN(16) : { *(.bss*) *(COMMON) } :text
}
