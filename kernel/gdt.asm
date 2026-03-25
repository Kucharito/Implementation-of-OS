gdt_start: ; nestranuj tieto labely, treba ich na vypocet velkosti a skokov
    ; GDT zacina nulovym 8-bajtovym zaznamom
    dd 0x0 ; 4 bajty
    dd 0x0 ; 4 bajty

; GDT pre kodovy segment. baza = 0x00000000, dlzka = 0xfffff
; vyznam flagov je v dokumente os-dev.pdf, strana 36
gdt_code:
    dw 0xffff    ; dlzka segmentu, bity 0-15
    dw 0x0       ; baza segmentu, bity 0-15
    db 0x0       ; baza segmentu, bity 16-23
    db 10011010b ; flagy (8 bitov)
    db 11001111b ; flagy (4 bity) + dlzka segmentu, bity 16-19
    db 0x0       ; baza segmentu, bity 24-31

; GDT pre datovy segment. baza a dlzka su rovnake ako pri kode
; niektore flagy su ine, pozri os-dev.pdf
gdt_data:
    dw 0xffff
    dw 0x0
    db 0x0
    db 10010010b
    db 11001111b
    db 0x0

gdt_end:

; GDT deskriptor
gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; velkost (16 bit), vzdy o 1 mensia nez skutocna
    dd gdt_start ; adresa (32 bit)

; definicia konstant pre neskorsie pouzitie
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start
