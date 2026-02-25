[bits 16]; use 16 bit mode
[org 0x7c00]; starting address when loaded by the BIOS

boot:
  mov si,msg; BIOS write character function
  mov ah,0x0e; BIOS teletype output function

loop1:
  lodsb
  cmp al,0 ; check if we reached the end of the string
  je halt
  int 0x10; call BIOS video interrupt to print the character
  jmp loop1; repeat for the next character

halt:
  hlt ; stop the CPU

msg: db "Booting super IVOS",0

times 510-($-$$) db 0;  fill the rest of the 512 byte sector with zeros
dw 0xaa55
