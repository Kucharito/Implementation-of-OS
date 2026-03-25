global _start;
[bits 32]

_start:
    [extern start_kernel] ; Definuje miesto volania, musi mat rovnaky nazov ako funkcia v C
    call start_kernel ; Zavola C funkciu, linker vie kde je v pamati
    jmp $
