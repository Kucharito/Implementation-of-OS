#include "ide.h"
#include "vga.h"
#include "serial.h"

/* Kernel adapter: FAT vrstva cita/zapisuje sektory cez IDE driver. */
int ata_read_sector(unsigned int lba, unsigned char *buffer)
{
    return ide_read_sector(lba, buffer);
}

/* Kernel adapter: FAT vrstva cita/zapisuje sektory cez IDE driver. */
int ata_write_sector(unsigned int lba, const unsigned char *buffer)
{
    return ide_write_sector(lba, buffer);
}

/* R/O CLI nepouziva stdin stream; ponechane ako stub. */
unsigned int fat_input_read(unsigned char *buffer, unsigned int max_len)
{
    (void)buffer;
    (void)max_len;
    return 0;
}

/* Konzolovy vystup FAT vrstvy smeruje na VGA aj serial pre debug. */
void console_putc(char c)
{
    vga_putchar(c);
    serial_putchar(c);
}

/* Vypis bloku textu cez adapter konzoly. */
void console_write(const char *buf, unsigned int len)
{
    unsigned int i;
    for (i = 0; i < len; i++)
    {
        console_putc(buf[i]);
    }
}
