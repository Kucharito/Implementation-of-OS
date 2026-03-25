#include <stdio.h>

#include "fat.h"
#include "fat_linux_adapter.h"

static FILE *disk;

/* Otvori disk image v read/write binarnom mode pre sektorovy pristup. */
int fat_linux_adapter_open(const char *path)
{
    disk = fopen(path, "r+b");
    return disk ? 0 : -1;
}

/* Zatvori aktualne otvoreny disk image handle. */
void fat_linux_adapter_close(void)
{
    if (disk)
        fclose(disk);
    disk = 0;
}

/* Precita presne jeden 512-bajtovy sektor z image na LBA offsete. */
int ata_read_sector(unsigned int lba, unsigned char *buffer)
{
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    return fread(buffer, SECTOR_SIZE, 1, disk);
}

/* Zapise presne jeden 512-bajtovy sektor do image na LBA offsete. */
int ata_write_sector(unsigned int lba, const unsigned char *buffer)
{
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    return fwrite(buffer, SECTOR_SIZE, 1, disk);
}

/* Precita vstupny payload pre write prikaz zo stdin streamu. */
unsigned int fat_input_read(unsigned char *buffer, unsigned int max_len)
{
    return (unsigned int)fread(buffer, 1, max_len, stdin);
}

/* Vypise jeden znak cez host konzolu. */
void console_putc(char c)
{
    putchar(c);
}

/* Vypise surovu bajtovu sekvenciu cez host konzolu. */
void console_write(const char *buf, unsigned int len)
{
    for (unsigned int i = 0; i < len; i++)
        putchar(buf[i]);
}