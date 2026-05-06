global isr_common_stub
extern isr_handler

[bits 32]

isr_common_stub:
    pusha
    mov eax, esp
    push eax
    call isr_handler
    add esp, 4
    mov esp, eax
    popa
    iret