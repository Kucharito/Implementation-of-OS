#include <stdio.h>
#include "fat.h"

void fat16_init();
void read_file(char *filename);
void dir_listing();

static FILE *disk;

int ata_read_sector(unsigned int lba, unsigned char *buffer)
{
    fseek(disk, lba * SECTOR_SIZE, SEEK_SET);
    return fread(buffer, SECTOR_SIZE, 1, disk);
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


int main(void)
{
    disk = fopen("sd.img", "rb");
    if (!disk) return 1;

    fat16_init();

    printf("=== ROOT DIR ===\n");
    dir_listing();

    printf("\n=== changeDir(\"ADR1\") ===\n");
    if (changeDir("ADR1") == 0)
    {
        dir_listing();
    }
    else
    {
        printf("changeDir failed\n");
    }

    printf("\n=== changeDir(\"..\") ===\n");
    if (changeDir("..") == 0)
    {
        dir_listing();
    }
    else
    {
        printf("changeDir back failed\n");
    }

    printf("\n=== read_file in ROOT ===\n");
    read_file("ABSTRAKT.TXT");

    fclose(disk);
    return 0;
}