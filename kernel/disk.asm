; nacita 'dh' sektorov z disku 'dl' do ES:BX
disk_load:
    pusha
    ; citanie z disku vyzaduje nastavit konkretne hodnoty v registroch
    ; preto sa prepisu vstupne parametre z 'dx', tak si ich ulozime
    ; na zasobnik pre neskorsie pouzitie.
    push dx

    mov ah, 0x02 ; ah <- funkcia prerusenia int 0x13, 0x02 = citanie
    mov al, dh   ; al <- pocet sektorov na nacitanie (0x01 .. 0x80)
    mov cl, 0x02 ; cl <- sektor (0x01 .. 0x11)
                 ; 0x01 je boot sektor, 0x02 je prvy volny sektor
    mov ch, 0x00 ; ch <- cylinder (0x0 .. 0x3FF, horne 2 bity su v 'cl')
    ; dl <- cislo disku, nastavi ho volajuci kod z BIOS hodnoty
    ; (0 = floppy, 1 = floppy2, 0x80 = hdd, 0x81 = hdd2)
    mov dh, 0x00 ; dh <- cislo hlavicky (0x0 .. 0xF)

    ; [es:bx] <- adresa buffera, kam sa maju data ulozit
    ; volajuci ju pripravi, je to standard pre int 13h
    int 0x13      ; BIOS prerusenie
    jc disk_error ; pri chybe je nastaveny carry bit

    pop dx
    cmp al, dh    ; BIOS nastavi 'al' na pocet nacitanych sektorov, porovname to
    jne sectors_error
    popa
    ret


disk_error:
    jmp disk_loop

sectors_error:
    jmp disk_loop

disk_loop:
    jmp $
