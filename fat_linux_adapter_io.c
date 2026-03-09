#include <stdio.h>

#include "fat.h"
#include "fat_linux_adapter.h"

static FILE *disk;

int fat_linux_adapter_open(const char *path)
{
    disk = fopen(path, "r+b");
    return disk ? 0 : -1;
}

void fat_linux_adapter_close(void)
{
    if (disk)
        fclose(disk);
    disk = 0;
}

int ata_read_sector(unsigned int lba, unsigned char *buffer)
{
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    return fread(buffer, SECTOR_SIZE, 1, disk);
}

int ata_write_sector(unsigned int lba, const unsigned char *buffer)
{
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    return fwrite(buffer, SECTOR_SIZE, 1, disk);
}

unsigned int fat_input_read(unsigned char *buffer, unsigned int max_len)
{
    return (unsigned int)fread(buffer, 1, max_len, stdin);
}

void console_putc(char c)
{
    putchar(c);
}

void console_write(const char *buf, unsigned int len)
{
    for (unsigned int i = 0; i < len; i++)
        putchar(buf[i]);
}