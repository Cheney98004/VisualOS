; disk.asm — allow kernel (32-bit PM) to write sectors using BIOS INT 13h

[BITS 16]
[ORG 0]

global rm_write_sectors
global rm_stub_end

rm_write_sectors:
    ; Expected registers (set by PM code before far call):
    ;   AX = number of sectors
    ;   BX = buffer offset
    ;   ES = buffer segment
    ;   DX = drive (0 = floppy)
    ;   CHS/LBA in memory (DAP)

    mov si, dap
    mov ah, 0x42
    int 0x13
    retf      ; return to Protected Mode stub

dap:
    db 0x10,0      ; size, reserved
    dw 0           ; sector count
    dw 0           ; buffer offset
    dw 0           ; buffer segment
    dd 0           ; LBA low
    dd 0           ; LBA high

rm_stub_end:
