[bits 16]
[org 0x7c00]

boot:
    mov si, msg
    mov ah, 0x13

    mov bp, msg
    mov al, 0x00
    mov cx, 21
    mov bx, 0x02
    mov dx,0

    int 0x10

halt:
    hlt

msg: db "Booting super IVOS",0

times 510-($-$$) db 0
dw 0xaa55