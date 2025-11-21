ENTRY(kmain)

SECTIONS
{
    /* Kernel 加載位置（bootloader 讀入到 0x10000） */
    . = 0x10000;

    /* ---- TEXT ---- */
    .text :
    {
        _kernel_start = .;

        *(.text.kmain)
        *(.text*)

        _text_end = .;
    }

    /* ---- RODATA ---- */
    .rodata :
    {
        *(.rodata*)
        _rodata_end = .;
    }

    /* ---- DATA ---- */
    .data :
    {
        _data_vma = .;
        *(.data*)
        _edata = .;
    }

    /* LMA = VMA - 0x10000 (kernel.bin 從 offset 0 開始) */
    _data_lma = LOADADDR(.data);

    /* ---- BSS ---- */
    .bss :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }

    _kernel_end = .;
}
