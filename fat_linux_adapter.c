#include <stdio.h>
#include "fat.h"

void fat16_init();
void read_file(char *filename);
void dir_listing();

static FILE *disk;

int ata_read_sector(u32 lba, u8 *buffer)
{
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    return fread(buffer, SECTOR_SIZE, 1, disk);
}

void console_putc(char c)
{
    putchar(c);
}

void console_write(const char *buf, u32 len)
{
    for (u32 i = 0; i < len; i++)
        putchar(buf[i]);
}

int main(int argc, char **argv)
{
    disk = fopen("sd.img", "rb");
    if (!disk) return 1;

    fat16_init();

    if (argc == 2)
        /* ./fat ABSTRAKT.TXT */
        read_file(argv[1]);
    else
        /* ./fat */
        dir_listing();

    fclose(disk);
    return 0;
}