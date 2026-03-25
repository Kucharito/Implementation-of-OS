[bits 16]
switch_to_32bit:
    cli ; 1. vypnut prerusenia
    lgdt [gdt_descriptor] ; 2. nacitat GDT deskriptor
    mov eax, cr0
    or eax, 0x1 ; 3. nastavit bit 32-bit rezimu v cr0
    mov cr0, eax
    jmp CODE_SEG:init_32bit ; 4. far skok do ineho segmentu

[bits 32]
init_32bit: ; odtialto bezime na 32-bit instrukciach
    mov ax, DATA_SEG ; 5. aktualizovat segmentove registre
    mov ds, ax
    mov ss, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    mov ebp, 0x90000 ; 6. nastavit zasobnik na vrch volneho priestoru
    mov esp, ebp

    call BEGIN_32BIT ; 7. zavolat znamy label s uzitocnym kodom
