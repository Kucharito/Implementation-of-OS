section .data
    msg db "Hello brasko!", 10
    len equ $ - msg

section .text
    global _start

_start:
    mov rax, 1          ; sys_write
    mov rdi, 1          ; stdout
    mov rsi, msg        ; adresa textu
    mov rdx, len        ; dlzka
    syscall

    mov rax, 60         ; sys_exit
    xor rdi, rdi        ; exit code 0
    syscall