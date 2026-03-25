[bits 16]
[org 0x7c00]

KERNEL_OFFSET equ 0x1000 ; Rovnaka adresa ako pri linkovani jadra

mov [BOOT_DRIVE], dl ; BIOS pri starte ulozi boot disk do registra 'dl'
mov bp, 0x9000
mov sp, bp

call load_kernel ; nacitanie jadra z disku
call switch_to_32bit ; vypne prerusenia, nacita GDT a skoci do 'BEGIN_PM'
jmp $ ; sem by sa nemalo dostat

%include "disk.asm"
%include "gdt.asm"
%include "switch-to-32bit.asm"

[bits 16]
load_kernel:
    mov bx, KERNEL_OFFSET ; citanie z disku do adresy 0x1000
    ; kernel.bin je vacsi ako 2 sektory, treba nacitat viac sektorov.
    mov dh, 9
    mov dl, [BOOT_DRIVE]
    call disk_load
    ret

[bits 32]
BEGIN_32BIT:
    call KERNEL_OFFSET ; odovzdanie riadenia jadru
    jmp $ ; ak sa jadro vrati, ostaneme tu


BOOT_DRIVE db 0 ; ulozenie do pamate, lebo register 'dl' sa moze prepisat

; vypln do 512 bajtov
times 510 - ($-$$) db 0
dw 0xaa55
