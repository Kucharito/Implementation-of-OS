[bits 16]
[org 0x7c00]

boot_message:
    mov si, msg
    mov ah, 0x13

    mov bp, msg
    mov al, 0x00
    mov cx, 21
    mov bx, 0x02
    mov dx,0

    int 0x10

load_sector:
    mov ax, 0x0000 ; load the next sector at 0x0000
    mov es,ax ; set ES to 0x0000

    mov bx, 0x7e00 ; load the next sector at 0x7e00
    mov al, 10 ; number of sectors to read
    mov ch, 0 ; CHS - cylinder 0
    mov cl, 2 ; sector 2 (the second sector)
    mov dl, 0 ; drive 0 (default)
    mov dh, 0 ; CHS - head 0

    mov ah,0x02 ; read desired sectors into memory
    int 0x13 ; int 13h - BIOS disk services 

halt:
    hlt

msg: db "Booting super IVOS",0
times 510-($-$$) db 0
dw 0xaa55