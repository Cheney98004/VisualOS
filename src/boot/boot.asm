[BITS 16]
[ORG 0x7C00]

%define BPS            512
%define ROOT_SECTS     14
%define ROOT_START     19
%define DATA_START     33

%define KERNEL_LOAD_SEG 0x1000
%define KERNEL_ENTRY    0x10000

jmp short start
nop

; ==========================
; BIOS Parameter Block (BPB)
; ==========================

OEMLabel        db "MSWIN4.1"
BPB_BytsPerSec  dw 512
BPB_SecPerClus  db 1
BPB_RsvdSecCnt  dw 1
BPB_NumFATs     db 2
BPB_RootEntCnt  dw 224
BPB_TotSec16    dw 2880
BPB_Media       db 0xF0
BPB_FATSz16     dw 9
BPB_SecPerTrk   dw 18
BPB_NumHeads    dw 2
BPB_HiddSec     dd 0
BPB_TotSec32    dd 0

BS_DrvNum       db 0
BS_Reserved1    db 0
BS_BootSig      db 0x29
BS_VolID        dd 0x12345678
BS_VolLab       db "NO NAME    "
BS_FilSysType   db "FAT16   "

; ==========================
; Code starts here
; ==========================

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl

    ; Load root directory
    mov bx, buffer
    mov cx, ROOT_START
    mov si, ROOT_SECTS

load_root:
    push cx
    push si
    call read_sector
    pop si
    pop cx

    add bx, BPS
    inc cx
    dec si
    jnz load_root

    ; Search for KERNEL.BIN
    mov si, buffer
    mov cx, 224

search_loop:
    cmp byte [si], 0
    je fail

    cmp byte [si+11], 0x0F
    je skip

    push si
    mov di, kernel_name
    mov dx, 11
    mov cx, dx
    repe cmpsb
    pop si

    je found

skip:
    add si, 32
    loop search_loop
    jmp fail

; ==========================
; Kernel found
; ==========================

found:
    mov ax, [si+26] ; first cluster
    mov [cluster], ax

    mov eax, [si+28]
    add eax, 511
    shr eax, 9
    mov [ksect], ax

    mov ax, KERNEL_LOAD_SEG
    mov es, ax
    xor bx, bx

    mov ax, [cluster]
    mov cx, [ksect]

load_kernel:
    push ax
    push cx

    mov dx, ax
    sub dx, 2
    mov ax, DATA_START
    add ax, dx
    mov cx, ax

    call read_sector

    add bx, BPS
    pop cx
    pop ax

    inc ax
    loop load_kernel

; ==========================
; Enter protected mode
; ==========================

    cli
    in al, 0x92
    or al, 2
    out 0x92, al

    lgdt [gdt_desc]

    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:protected

; ==========================
; Protected mode
; ==========================

[BITS 32]
protected:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov esp, 0x90000

    jmp 0x08:KERNEL_ENTRY

; ==========================
; Data
; ==========================

[BITS 16]

boot_drive  db 0
cluster     dw 0
ksect       dw 0
kernel_name db "KERNEL  BIN"

buffer equ 0x0500

dap:
    db 0x10, 0
    dw 1, 0, 0
    dd 0
    dd 0

; ==========================
; Disk read (INT 13h EXT)
; ==========================

read_sector:
    pusha

    mov byte [dap], 0x10
    mov word [dap+4], bx
    mov word [dap+6], es
    mov dword [dap+8], ecx
    mov dword [dap+12], 0

    mov dl, [boot_drive]
    mov si, dap
    mov ah, 0x42
    int 0x13

    popa
    ret

; ==========================
; Fail handler
; ==========================

fail:
    cli
    hlt
    jmp $

; ==========================
; GDT
; ==========================

gdt:
    dq 0
    dq 0x00CF9A000000FFFF
    dq 0x00CF92000000FFFF

gdt_desc:
    dw gdt_desc - gdt - 1
    dd gdt

times 510-($-$$) db 0
dw 0xAA55
