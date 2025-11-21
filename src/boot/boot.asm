; Minimal 16-bit boot sector that switches to 32-bit protected mode and jumps to the kernel
; Assembles with: nasm -f bin boot.asm -o boot.bin

[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00

    mov [BOOT_DRIVE], dl          ; Preserve BIOS boot drive

    mov si, msg
    call puts

    ; Load kernel from sector 2 onward into 0x10000 (ES:BX = 0x1000:0)
    mov bx, 0x1000
    mov es, bx
    xor bx, bx
    mov ah, 0x02                  ; BIOS: read sectors
    mov al, 20                    ; sectors to read (adjust if kernel grows)
    mov ch, 0
    mov cl, 2                     ; start reading at LBA 1 -> CHS sector 2
    mov dh, 0
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error

    ; Enable A20
    in al, 0x92
    or al, 00000010b
    out 0x92, al

    ; Load GDT and enter protected mode
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp CODE_SEL:pm_entry

; Print zero-terminated string at DS:SI using BIOS teletype
puts:
    lodsb
    or al, al
    jz .done
    mov ah, 0x0E
    mov bx, 0x0007
    int 0x10
    jmp puts
.done:
    ret

disk_error:
    mov si, disk_msg
    call puts
    cli
    hlt

[BITS 32]
pm_entry:
    mov ax, DATA_SEL
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000

    jmp dword CODE_SEL:KERNEL_ENTRY

; -------------------------------
; Data and tables
; -------------------------------
ALIGN 8
gdt:
    dq 0                        ; Null descriptor
    dq 0x00CF9A000000FFFF       ; Code segment
    dq 0x00CF92000000FFFF       ; Data segment

gdt_descriptor:
    dw gdt_descriptor - gdt - 1
    dd gdt

msg db "Booting VisualOS...", 0
disk_msg db "Disk error", 0
BOOT_DRIVE db 0

CODE_SEL equ gdt + 8 - gdt      ; 0x08
DATA_SEL equ gdt + 16 - gdt     ; 0x10
KERNEL_ENTRY equ 0x0010000

times 510-($-$$) db 0
dw 0xAA55
